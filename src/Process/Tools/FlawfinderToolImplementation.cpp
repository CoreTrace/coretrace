// SPDX-License-Identifier: Apache-2.0
#include "Process/Tools/AnalysisTools.hpp"
#include "Process/Tools/Sarif.hpp"

#include <coretrace/logger.hpp>

namespace ctrace
{
    std::vector<std::string>
    FlawfinderToolImplementation::buildArguments(const ctrace::ProgramConfig& config,
                                                 const std::string& file)
    {
        // Always the machine format (flawfinder >= 2.0): CoreTrace renders the text itself.
        std::vector<std::string> args = {"--sarif"};
        appendToolArguments(args, config, "flawfinder");
        args.push_back(file);
        return args;
    }

    std::optional<std::vector<Diagnostic>>
    FlawfinderToolImplementation::parseDiagnostics(const std::string& output)
    {
        return diagnosticsFromSarifText(output, "flawfinder");
    }

    void FlawfinderToolImplementation::execute(const std::string& file,
                                               const ctrace::ProgramConfig& config,
                                               ToolOutput& output) const
    {
        coretrace::log(coretrace::Level::Info, "Running flawfinder on {}\n", file);
        const auto run = runExternalTool(config, *this, buildArguments(config, file), output);
        if (!run)
        {
            return;
        }
        const auto diagnostics = parseDiagnostics(run->output);
        std::string text;
        if (config.output.sarif_format || !diagnostics)
        {
            text = run->output;
        }
        else if (!diagnostics->empty())
        {
            text = renderLines(*diagnostics);
        }
        if (!text.empty())
        {
            if (config.runtime.ipc == "standardIO" || !ipc)
            {
                output.result(text);
            }
            else
            {
                ipc->write(text);
                output.record("stdout", text);
            }
        }
        if (diagnostics)
        {
            output.diagnostics(*diagnostics);
        }
        else
        {
            coretrace::log(coretrace::Level::Warn, coretrace::Module(name()),
                           "output is not a SARIF document (flawfinder >= 2.0 is required); "
                           "findings are not counted\n");
        }
    }

    std::string FlawfinderToolImplementation::name() const
    {
        return "flawfinder";
    }
} // namespace ctrace
