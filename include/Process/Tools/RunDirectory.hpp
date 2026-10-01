// SPDX-License-Identifier: Apache-2.0
#ifndef RUN_DIRECTORY_HPP
#define RUN_DIRECTORY_HPP

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

#include <unistd.h>

namespace ctrace
{
    /// A directory of its own for what one tool run writes, removed with this object: runs in
    /// parallel never share a path, and nothing lands in the working directory.
    class RunDirectory
    {
      public:
        /// `prefix` names the directory in the temporary directory, e.g. "ctrace-runtime".
        explicit RunDirectory(const std::string& prefix)
        {
            std::string pattern =
                (std::filesystem::temp_directory_path() / (prefix + "-XXXXXX")).string();
            if (::mkdtemp(pattern.data()) == nullptr)
            {
                throw std::runtime_error("cannot create a run directory: " +
                                         std::string(std::strerror(errno)));
            }
            path_ = pattern;
        }

        ~RunDirectory()
        {
            std::error_code err;
            std::filesystem::remove_all(path_, err);
        }

        RunDirectory(const RunDirectory&) = delete;
        RunDirectory& operator=(const RunDirectory&) = delete;

        [[nodiscard]] const std::filesystem::path& path() const noexcept
        {
            return path_;
        }

      private:
        std::filesystem::path path_;
    };
} // namespace ctrace

#endif // RUN_DIRECTORY_HPP
