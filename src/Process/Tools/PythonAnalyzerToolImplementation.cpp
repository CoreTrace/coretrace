// SPDX-License-Identifier: Apache-2.0
#include "Process/Tools/AnalysisTools.hpp"
#include "Process/Tools/Sarif.hpp"

#include <coretrace/logger.hpp>

#include <filesystem>

namespace ctrace
{
    namespace
    {
        constexpr const char* kToolName = "coretrace-python-analyzer";

        /// The analyzer's exit codes that mean it analyzed the file: 0 clean, 1 findings.
        /// 2 is an analysis error and is reported as a failed run.
        constexpr std::initializer_list<int> kCompletedExitCodes = {0, 1};
    } // namespace

    std::vector<std::string>
    PythonAnalyzerToolImplementation::buildArguments(const ctrace::ProgramConfig& config,
                                                     const std::string& file)
    {
        // Always SARIF: CoreTrace renders the text itself and merges SARIF across tools.
        std::vector<std::string> args = {"--check", "--format", "sarif"};
        appendToolArguments(args, config, kToolName);
        args.push_back(file);
        return args;
    }

    std::optional<std::vector<Diagnostic>>
    PythonAnalyzerToolImplementation::parseDiagnostics(const std::string& output,
                                                       const std::string& file)
    {
        auto diagnostics = diagnosticsFromSarifText(output, kToolName);
        if (!diagnostics)
        {
            return diagnostics;
        }
        const std::filesystem::path directory = std::filesystem::path(file).parent_path();
        for (Diagnostic& diagnostic : *diagnostics)
        {
            if (!diagnostic.file.empty() && std::filesystem::path(diagnostic.file).is_relative())
            {
                diagnostic.file = (directory / diagnostic.file).lexically_normal().string();
            }
        }
        return diagnostics;
    }

    void PythonAnalyzerToolImplementation::execute(const std::string& file,
                                                   const ctrace::ProgramConfig& config,
                                                   ToolOutput& output) const
    {
        coretrace::log(coretrace::Level::Info, "Running {} on {}\n", kToolName, file);
        const auto run = runExternalTool(config, *this, buildArguments(config, file), output,
                                         kCompletedExitCodes);
        if (!run)
        {
            return;
        }
        const auto diagnostics = parseDiagnostics(run->output, file);
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
            return;
        }
        // In SARIF mode the merged document is the output; text lines would pollute it.
        if (!config.output.sarif_format && !diagnostics->empty())
        {
            output.result(renderLines(*diagnostics));
        }
        output.diagnostics(*diagnostics);
    }

    std::string PythonAnalyzerToolImplementation::name() const
    {
        return kToolName;
    }
} // namespace ctrace
