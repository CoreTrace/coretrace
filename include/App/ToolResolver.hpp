// SPDX-License-Identifier: Apache-2.0
#ifndef APP_TOOL_RESOLVER_HPP
#define APP_TOOL_RESOLVER_HPP

#if defined(_WIN32)
#define NOMINMAX
// Without this, windows.h drags in the original winsock.h and every later
// include of winsock2.h redefines sockaddr.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <cstdlib>
#include <system_error>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace ctrace
{
    struct ResolvedToolCommand
    {
        std::string executable;
        std::vector<std::string> prefixArguments;
    };

    namespace detail
    {
        [[nodiscard]] inline bool hasEnvValue(const char* name)
        {
            const char* value = std::getenv(name);
            return value != nullptr && *value != '\0';
        }

        [[nodiscard]] inline std::string envOrDefault(const char* name, std::string_view fallback)
        {
            const char* value = std::getenv(name);
            if (value == nullptr || *value == '\0')
            {
                return std::string(fallback);
            }
            return value;
        }

        /// The directory holding the running executable, or an empty path when
        /// the platform will not say.
        [[nodiscard]] inline std::filesystem::path executableDirectory()
        {
#if defined(_WIN32)
            std::wstring buffer(MAX_PATH, wchar_t{});
            for (;;)
            {
                const DWORD written =
                    GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
                if (written == 0)
                {
                    return {};
                }
                if (written < buffer.size())
                {
                    buffer.resize(written);
                    break;
                }
                buffer.resize(buffer.size() * 2);
            }
            return std::filesystem::path(buffer).parent_path();
#elif defined(__linux__)
            std::error_code ec;
            const auto self = std::filesystem::read_symlink("/proc/self/exe", ec);
            return ec ? std::filesystem::path{} : self.parent_path();
#else
            return {};
#endif
        }

        /// A path beside the running executable, where anything shipped with
        /// ctrace lives. The current directory is the project under analysis
        /// and says nothing about where ctrace was installed.
        [[nodiscard]] inline std::filesystem::path executableRelativePath(std::string_view relative)
        {
            const auto directory = executableDirectory();
            if (directory.empty())
            {
                return {};
            }
            return directory / std::filesystem::path(relative);
        }

        [[nodiscard]] inline std::string repoLocalExecutable(std::string_view relativeStem)
        {
            std::filesystem::path path(relativeStem);
#ifdef _WIN32
            path += ".exe";
#endif
            return path.make_preferred().string();
        }
    } // namespace detail

    [[nodiscard]] inline ResolvedToolCommand resolveCppcheckCommand()
    {
        return {detail::envOrDefault("CORETRACE_CPPCHECK_BIN", "cppcheck"), {}};
    }

    [[nodiscard]] inline ResolvedToolCommand resolveTscancodeCommand()
    {
#ifdef _WIN32
        constexpr std::string_view fallback = "tscancode";
#else
        const std::string fallback = detail::repoLocalExecutable("./tscancode/src/tscancode/trunk/tscancode");
#endif
        return {detail::envOrDefault("CORETRACE_TSCANCODE_BIN",
                                     fallback),
                {}};
    }

    [[nodiscard]] inline ResolvedToolCommand resolveIkosCommand()
    {
#ifdef _WIN32
        constexpr std::string_view fallback = "ikos";
#else
        const std::string fallback = detail::repoLocalExecutable("./ikos/src/ikos-build/bin/ikos");
#endif
        return {detail::envOrDefault("CORETRACE_IKOS_BIN",
                                     fallback),
                {}};
    }

    /**
     * @brief How flawfinder is run.
     *
     * The bundled script is looked for beside the ctrace executable, never
     * relative to the current directory: a run analyses the reader's project
     * from the reader's project, and a bare "./flawfinder/..." pointed there
     * instead of at the installation, so flawfinder was reported missing from
     * every workspace that did not happen to contain a copy of it.
     *
     * Order: an explicit override, then the script shipped with ctrace, then
     * flawfinder as an installed module, which is how pip installs it.
     */
    [[nodiscard]] inline ResolvedToolCommand resolveFlawfinderCommand()
    {
        const bool hasCustomLauncher = detail::hasEnvValue("CORETRACE_FLAWFINDER_LAUNCHER");
#ifdef _WIN32
        const std::string pythonExe = detail::envOrDefault("CORETRACE_PYTHON_BIN", "python");
#else
        const std::string pythonExe = detail::envOrDefault("CORETRACE_PYTHON_BIN", "python3");
#endif

        ResolvedToolCommand command{detail::envOrDefault("CORETRACE_FLAWFINDER_LAUNCHER", pythonExe),
                                    {}};
        if (hasCustomLauncher)
        {
            return command;
        }

        if (detail::hasEnvValue("CORETRACE_FLAWFINDER_SCRIPT"))
        {
            command.prefixArguments.push_back(detail::envOrDefault("CORETRACE_FLAWFINDER_SCRIPT", ""));
            return command;
        }

        const auto bundled = detail::executableRelativePath("flawfinder/flawfinder.py");
        std::error_code ignored;
        if (std::filesystem::exists(bundled, ignored))
        {
            command.prefixArguments.push_back(bundled.string());
            return command;
        }

        // pip install flawfinder puts it on the module path.
        command.prefixArguments.emplace_back("-m");
        command.prefixArguments.emplace_back("flawfinder");
        return command;
    }
} // namespace ctrace

#endif // APP_TOOL_RESOLVER_HPP
