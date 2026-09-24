// SPDX-License-Identifier: Apache-2.0
#include "App/ShippedDefaults.hpp"

#include "App/SupportedTools.hpp"

#include <algorithm>
#include <array>
#include <system_error>

namespace ctrace
{
    namespace
    {
        struct DefaultModel
        {
            const char* key; ///< Config key, for the warning.
            const char* relativePath;
            std::string StackAnalyzerConfig::* member;
        };

        constexpr std::array<DefaultModel, 3> kDefaultModels = {{
            {"stack_analyzer.resource_model", "resource-lifetime/generic.txt",
             &StackAnalyzerConfig::resource_model},
            {"stack_analyzer.escape_model", "stack-escape/generic.txt",
             &StackAnalyzerConfig::escape_model},
            {"stack_analyzer.buffer_model", "buffer-overflow/generic.txt",
             &StackAnalyzerConfig::buffer_model},
        }};

        [[nodiscard]] bool selectsStackAnalyzer(const ProgramConfig& config)
        {
            return config.analysis.static_enabled ||
                   std::find(config.analysis.invoke.begin(), config.analysis.invoke.end(),
                             "ctrace_stack_analyzer") != config.analysis.invoke.end();
        }

        /// Where a file shipped with the binary is looked for, in order: under the install
        /// prefix (`<exe dir>/..`), then in the build tree (`<exe dir>`).
        [[nodiscard]] std::array<std::filesystem::path, 2>
        nextToExecutable(const std::filesystem::path& executable,
                         const std::filesystem::path& relative)
        {
            const std::filesystem::path exeDir = executable.parent_path();
            return {(exeDir / ".." / relative).lexically_normal(),
                    (exeDir / relative).lexically_normal()};
        }

        [[nodiscard]] bool isExecutableFile(const std::filesystem::path& path)
        {
            using std::filesystem::perms;
            std::error_code err;
            const auto status = std::filesystem::status(path, err);
            return !err && std::filesystem::is_regular_file(status) &&
                   (status.permissions() &
                    (perms::owner_exec | perms::group_exec | perms::others_exec)) != perms::none;
        }
    } // namespace

    CT_NODISCARD std::filesystem::path
    defaultModelsDirectory(const std::filesystem::path& executable)
    {
        for (const auto& candidate : nextToExecutable(executable, "config/models"))
        {
            std::error_code err;
            if (std::filesystem::is_directory(candidate, err))
            {
                return candidate;
            }
        }
        return {};
    }

    void applyBundledTools(ProgramConfig& config, const std::filesystem::path& executable)
    {
        for (const std::string_view toolName : SUPPORTED_TOOLS)
        {
            const std::string tool(toolName);
            const auto configured = config.tools.paths.find(tool);
            if (tool == "ctrace_stack_analyzer" ||
                (configured != config.tools.paths.end() && !configured->second.empty()))
            {
                continue; // In-process, or located by the configuration.
            }
            for (const auto& candidate : nextToExecutable(
                     executable, std::filesystem::path("libexec/coretrace") / tool / tool))
            {
                if (isExecutableFile(candidate))
                {
                    config.tools.paths[tool] = candidate.string();
                    break;
                }
            }
        }
    }

    void applyDefaultModels(ProgramConfig& config, const std::filesystem::path& modelsDir,
                            std::vector<std::string>& warnings)
    {
        if (!selectsStackAnalyzer(config))
        {
            return;
        }
        for (const DefaultModel& model : kDefaultModels)
        {
            std::string& value = config.stack_analyzer.*model.member;
            if (!value.empty())
            {
                continue;
            }
            const std::filesystem::path candidate = modelsDir / model.relativePath;
            std::error_code err;
            if (!modelsDir.empty() && std::filesystem::is_regular_file(candidate, err))
            {
                value = candidate.lexically_normal().string();
            }
            else
            {
                const std::string where =
                    modelsDir.empty()
                        ? "no config/models directory next to the executable"
                        : "no file at '" + candidate.lexically_normal().string() + "'";
                warnings.push_back(std::string(model.key) + " is not set and " + where +
                                   "; the rules driven by this model stay inactive.");
            }
        }
    }

    void applyShippedDefaults(ProgramConfig& config, const std::filesystem::path& executable,
                              std::vector<std::string>& warnings)
    {
        if (executable.empty())
        {
            // Nothing is known to ship: `<exe dir>/..` would resolve against the working
            // directory instead of the installation.
            applyDefaultModels(config, {}, warnings);
            return;
        }
        applyDefaultModels(config, defaultModelsDirectory(executable), warnings);
        applyBundledTools(config, executable);
    }
} // namespace ctrace
