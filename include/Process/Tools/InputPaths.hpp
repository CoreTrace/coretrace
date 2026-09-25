// SPDX-License-Identifier: Apache-2.0
#ifndef INPUT_PATHS_HPP
#define INPUT_PATHS_HPP

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace ctrace
{
    /// How the file of a finding is spelled, the same for every tool: an input as the user typed
    /// it; any other file relative to the working directory when below it, else absolute. Tools
    /// report paths their own way (absolute, relative to a project root, differently from one
    /// rule to the next); one spelling per file keeps the console, the API and the SARIF
    /// fingerprints consistent across tools and machines.
    class InputPaths
    {
      public:
        explicit InputPaths(const std::vector<std::string>& inputs)
            : workingDirectory_(identityOf(std::filesystem::current_path()))
        {
            for (const std::string& input : inputs)
            {
                inputs_.emplace(identityOf(input), input);
            }
        }

        /// The input `file` is, as the user spelled it. A relative `file` is resolved against
        /// the working directory.
        [[nodiscard]] std::optional<std::string> asInput(const std::filesystem::path& file) const
        {
            if (const auto input = inputs_.find(identityOf(file)); input != inputs_.end())
            {
                return input->second;
            }
            return std::nullopt;
        }

        /// `file` as findings show it (see the class comment).
        [[nodiscard]] std::string display(const std::filesystem::path& file) const
        {
            if (auto input = asInput(file))
            {
                return *input;
            }
            const std::filesystem::path identity = identityOf(file);
            const std::filesystem::path relative = identity.lexically_relative(workingDirectory_);
            const bool below = !relative.empty() && *relative.begin() != "..";
            return (below ? relative : identity).string();
        }

        /// One spelling per file: absolute, `.` and `..` resolved, symlinks followed where the
        /// path exists.
        [[nodiscard]] static std::filesystem::path identityOf(const std::filesystem::path& path)
        {
            const std::filesystem::path absolute = std::filesystem::absolute(path);
            std::error_code err;
            const std::filesystem::path resolved = std::filesystem::weakly_canonical(absolute, err);
            return (err ? absolute : resolved).lexically_normal();
        }

      private:
        std::map<std::filesystem::path, std::string> inputs_;
        std::filesystem::path workingDirectory_;
    };
} // namespace ctrace

#endif // INPUT_PATHS_HPP
