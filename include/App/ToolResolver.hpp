// SPDX-License-Identifier: Apache-2.0
#ifndef APP_TOOL_RESOLVER_HPP
#define APP_TOOL_RESOLVER_HPP

#include <cstdlib>
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
        const std::string fallback =
            detail::repoLocalExecutable("./tscancode/src/tscancode/trunk/tscancode");
#endif
        return {detail::envOrDefault("CORETRACE_TSCANCODE_BIN", fallback), {}};
    }

    [[nodiscard]] inline ResolvedToolCommand resolveIkosCommand()
    {
#ifdef _WIN32
        constexpr std::string_view fallback = "ikos";
#else
        const std::string fallback = detail::repoLocalExecutable("./ikos/src/ikos-build/bin/ikos");
#endif
        return {detail::envOrDefault("CORETRACE_IKOS_BIN", fallback), {}};
    }

    [[nodiscard]] inline ResolvedToolCommand resolveFlawfinderCommand()
    {
        const bool hasCustomLauncher = detail::hasEnvValue("CORETRACE_FLAWFINDER_LAUNCHER");
#ifdef _WIN32
        const std::string pythonExe = detail::envOrDefault("CORETRACE_PYTHON_BIN", "python");
#else
        const std::string pythonExe = detail::envOrDefault("CORETRACE_PYTHON_BIN", "python3");
#endif

        ResolvedToolCommand command{
            detail::envOrDefault("CORETRACE_FLAWFINDER_LAUNCHER", pythonExe), {}};

        const std::string scriptPath = detail::envOrDefault(
            "CORETRACE_FLAWFINDER_SCRIPT", "./flawfinder/src/flawfinder-build/flawfinder.py");
        if (!hasCustomLauncher)
        {
            command.prefixArguments.push_back(scriptPath);
        }
        return command;
    }
} // namespace ctrace

#endif // APP_TOOL_RESOLVER_HPP
