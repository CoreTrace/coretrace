// SPDX-License-Identifier: Apache-2.0
#ifndef INPUT_PATHS_HPP
#define INPUT_PATHS_HPP

#include "Process/Tools/Diagnostic.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace ctrace
{
    /// `path` spelled so that no tool reads it as an option: a relative path that starts with
    /// `-` gets `./` in front of it, which lexically_normal() drops. Inputs are spelled so before
    /// they reach the tools, and findings so that they show the same spelling.
    [[nodiscard]] inline std::string notAnOption(std::string path)
    {
        if (path.starts_with('-'))
        {
            path.insert(0, "./");
        }
        return path;
    }

    /// How the file of a finding is spelled, the same for every tool: an input as the user typed
    /// it; any other file relative to the working directory when below it, with `./` kept before
    /// a `-` (notAnOption), else absolute. Tools report paths their own way (absolute, relative
    /// to a project root, differently from one rule to the next); one spelling per file keeps
    /// the console, the API and the SARIF fingerprints consistent across tools and machines.
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

        /// `file` as findings show it (see the class comment). A finding without a file keeps
        /// none: an empty path is not the working directory.
        [[nodiscard]] std::string display(const std::filesystem::path& file) const
        {
            if (file.empty())
            {
                return {};
            }
            // Resolved once: asInput() would resolve it again for a file that is not an input.
            const std::filesystem::path identity = identityOf(file);
            if (const auto input = inputs_.find(identity); input != inputs_.end())
            {
                return input->second;
            }
            const std::filesystem::path relative = identity.lexically_relative(workingDirectory_);
            const bool below = !relative.empty() && *relative.begin() != "..";
            return below ? notAnOption(relative.string()) : identity.string();
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

    /// Spells the file of every finding, and of each place it involves, as InputPaths::display()
    /// does, for a tool that reports the files it was given its own way: cppcheck and tscancode
    /// rewrite the paths of their file list, the runtime analyzer names its source. Resolves
    /// nothing when there is no finding.
    inline void respellFindings(std::vector<Diagnostic>& diagnostics,
                                const std::vector<std::string>& inputs)
    {
        if (diagnostics.empty())
        {
            return;
        }
        const InputPaths paths(inputs);
        for (Diagnostic& diagnostic : diagnostics)
        {
            diagnostic.file = paths.display(diagnostic.file);
            for (RelatedLocation& related : diagnostic.relatedLocations)
            {
                if (!related.file.empty())
                {
                    related.file = paths.display(related.file);
                }
            }
        }
    }
} // namespace ctrace

#endif // INPUT_PATHS_HPP
