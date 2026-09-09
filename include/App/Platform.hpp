// SPDX-License-Identifier: Apache-2.0
#ifndef APP_PLATFORM_HPP
#define APP_PLATFORM_HPP

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <string>

namespace ctrace::platform
{
    [[nodiscard]] inline bool isWindows() noexcept
    {
#ifdef _WIN32
        return true;
#else
        return false;
#endif
    }

    [[nodiscard]] inline std::string defaultIpcPath()
    {
#ifdef _WIN32
        const char* tempDir = std::getenv("TEMP");
        std::filesystem::path baseDir;
        if (tempDir != nullptr && *tempDir != '\0')
        {
            baseDir = tempDir;
        }
        else
        {
            try
            {
                baseDir = std::filesystem::temp_directory_path();
            }
            catch (const std::exception&)
            {
                baseDir = ".";
            }
        }
        return (baseDir / "coretrace_ipc.sock").string();
#else
        return "/tmp/coretrace_ipc.sock";
#endif
    }
} // namespace ctrace::platform

#endif // APP_PLATFORM_HPP
