// SPDX-License-Identifier: Apache-2.0
#include "Process/Tools/AnalysisTools.hpp"
#include "Process/Tools/Sarif.hpp"

#include <coretrace/logger.hpp>

#include <array>
#include <map>

namespace ctrace
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr const char* kToolName = "coretrace-python-analyzer";

        /// The analyzer's exit codes that mean it analyzed the project: 0 clean, 1 findings.
        /// 2 is an analysis error and is reported as a failed run.
        constexpr std::initializer_list<int> kCompletedExitCodes = {0, 1};

        constexpr std::array<const char*, 4> kProjectMarkers = {"pyproject.toml", "setup.py",
                                                                "setup.cfg", ".git"};

        /// One spelling per file, whatever the input said: absolute, `.` and `..` resolved,
        /// symlinks followed where the path exists.
        [[nodiscard]] fs::path identityOf(const fs::path& path)
        {
            const fs::path absolute = fs::absolute(path);
            std::error_code err;
            const fs::path resolved = fs::weakly_canonical(absolute, err);
            return (err ? absolute : resolved).lexically_normal();
        }
    } // namespace

    std::vector<std::string>
    PythonAnalyzerToolImplementation::buildArguments(const ctrace::ProgramConfig& config,
                                                     const std::string& projectRoot)
    {
        // Always SARIF: CoreTrace renders the text itself and merges SARIF across tools.
        std::vector<std::string> args = {"--check", "--format", "sarif"};
        appendToolArguments(args, config, kToolName);
        args.push_back(projectRoot);
        return args;
    }

    fs::path PythonAnalyzerToolImplementation::projectRoot(const std::string& file)
    {
        const fs::path directory = identityOf(file).parent_path();
        for (fs::path candidate = directory; !candidate.empty();
             candidate = candidate.parent_path())
        {
            for (const char* marker : kProjectMarkers)
            {
                std::error_code err;
                if (fs::exists(candidate / marker, err))
                {
                    return candidate;
                }
            }
            if (candidate == candidate.root_path())
            {
                break;
            }
        }
        return directory;
    }

    std::optional<std::vector<Diagnostic>> PythonAnalyzerToolImplementation::parseDiagnostics(
        const std::string& output, const fs::path& root, const std::vector<std::string>& inputs)
    {
        auto diagnostics = diagnosticsFromSarifText(output, kToolName);
        if (!diagnostics)
        {
            return diagnostics;
        }
        std::map<fs::path, std::string> inputByIdentity;
        for (const std::string& input : inputs)
        {
            inputByIdentity.emplace(identityOf(input), input);
        }
        const fs::path workingDirectory = identityOf(fs::current_path());
        std::vector<Diagnostic> kept;
        for (Diagnostic& diagnostic : *diagnostics)
        {
            if (diagnostic.file.empty())
            {
                continue;
            }
            // The analyzer locates findings relative to the project root it was given.
            const fs::path located =
                identityOf(fs::path(diagnostic.file).is_relative() ? root / diagnostic.file
                                                                   : fs::path(diagnostic.file));
            if (const auto input = inputByIdentity.find(located); input != inputByIdentity.end())
            {
                diagnostic.file = input->second;
                kept.push_back(std::move(diagnostic));
            }
            else if (located.extension() != ".py")
            {
                // Not in a Python source: a finding about the project itself, such as a
                // vulnerable pin in requirements.txt. It concerns every input of the project.
                const fs::path relative = located.lexically_relative(workingDirectory);
                const bool below = !relative.empty() && *relative.begin() != "..";
                diagnostic.file = (below ? relative : located).string();
                kept.push_back(std::move(diagnostic));
            }
        }
        return kept;
    }

    void PythonAnalyzerToolImplementation::executeBatch(const std::vector<std::string>& files,
                                                        const ctrace::ProgramConfig& config,
                                                        ToolOutput& output) const
    {
        std::map<fs::path, std::vector<std::string>> inputsByProject;
        for (const std::string& file : files)
        {
            inputsByProject[projectRoot(file)].push_back(file);
        }

        for (const auto& [root, inputs] : inputsByProject)
        {
            coretrace::log(coretrace::Level::Info, "Running {} on {} for {} input file(s)\n",
                           kToolName, root.string(), inputs.size());
            const auto run = runExternalTool(config, *this, buildArguments(config, root.string()),
                                             output, kCompletedExitCodes);
            if (!run)
            {
                continue;
            }
            const auto diagnostics = parseDiagnostics(run->output, root, inputs);
            if (!diagnostics)
            {
                coretrace::log(coretrace::Level::Warn, coretrace::Module(name()),
                               "output is not a SARIF document; findings are not counted\n");
                if (config.output.sarif_format)
                {
                    output.record("stdout", run->output);
                }
                else
                {
                    output.result(run->output);
                }
                continue;
            }
            // In SARIF mode the merged document is the output; text lines would pollute it.
            if (!config.output.sarif_format && !diagnostics->empty())
            {
                output.result(renderLines(*diagnostics));
            }
            output.diagnostics(*diagnostics);
        }
    }

    std::string PythonAnalyzerToolImplementation::name() const
    {
        return kToolName;
    }
} // namespace ctrace
