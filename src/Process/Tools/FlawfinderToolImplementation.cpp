// SPDX-License-Identifier: Apache-2.0
#include "Process/Tools/AnalysisTools.hpp"
#include "Process/Tools/Sarif.hpp"

#include <coretrace/logger.hpp>

namespace ctrace
{
    std::vector<std::string>
    FlawfinderToolImplementation::buildArguments(const ctrace::ProgramConfig& config,
                                                 const std::vector<std::string>& files)
    {
        // Always the machine format (flawfinder >= 2.0): CoreTrace renders the text itself.
        std::vector<std::string> args = {"--sarif"};
        appendToolArguments(args, config, "flawfinder");
        // flawfinder has no file list: the paths go on the command line, which bounds a run by
        // the system's argument size (about 2 MiB on Linux, tens of thousands of paths).
        args.insert(args.end(), files.begin(), files.end());
        return args;
    }

    std::optional<std::vector<Diagnostic>>
    FlawfinderToolImplementation::parseDiagnostics(const std::string& output)
    {
        return diagnosticsFromSarifText(output, "flawfinder");
    }

    void FlawfinderToolImplementation::executeBatch(const std::vector<std::string>& files,
                                                    const ctrace::ProgramConfig& config,
                                                    ToolOutput& output) const
    {
        coretrace::log(coretrace::Level::Info, "Running flawfinder on {}\n",
                       ctrace_tools::strings::joinByComma(files));
        const auto run = runExternalTool(config, *this, buildArguments(config, files), output, {0},
                                         toolTimeout(config, name(), files.size()));
        if (!run)
        {
            return;
        }
        const auto diagnostics = parseDiagnostics(run->output);
        if (!diagnostics)
        {
            coretrace::log(coretrace::Level::Warn, coretrace::Module(name()),
                           "output is not a SARIF document (flawfinder >= 2.0 is required); "
                           "findings are not counted\n");
            emit(config, output, run->output, /*toConsole=*/!config.output.sarif_format);
            return;
        }
        // In SARIF mode the merged document is the output; text lines would pollute it.
        if (!config.output.sarif_format && !diagnostics->empty())
        {
            emit(config, output, renderLines(*diagnostics), /*toConsole=*/true);
        }
        output.diagnostics(*diagnostics);
    }

    /// Text goes to the deprecated socket when one is configured, else to the sink; a text
    /// that must not reach stdout (raw output in SARIF mode) is only recorded.
    void FlawfinderToolImplementation::emit(const ctrace::ProgramConfig& config, ToolOutput& output,
                                            const std::string& text, bool toConsole) const
    {
        if (config.runtime.ipc != "standardIO" && ipc)
        {
            ipc->write(text);
            output.record("stdout", text);
        }
        else if (toConsole)
        {
            output.result(text);
        }
        else
        {
            output.record("stdout", text);
        }
    }

    std::string FlawfinderToolImplementation::name() const
    {
        return "flawfinder";
    }
} // namespace ctrace
