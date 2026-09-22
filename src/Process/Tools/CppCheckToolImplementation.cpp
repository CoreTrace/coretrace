// SPDX-License-Identifier: Apache-2.0
#include "Process/Tools/AnalysisTools.hpp"
#include "Process/Tools/Sarif.hpp"

#include <coretrace/logger.hpp>

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace ctrace
{
    namespace
    {
        /// cppcheck's own severity vocabulary. Everything that is not an error or plain
        /// information is a warning-class finding (style, performance, portability).
        [[nodiscard]] Severity severityOf(const std::string& word)
        {
            if (word == "error")
            {
                return Severity::Error;
            }
            if (word == "information" || word == "debug")
            {
                return Severity::Info;
            }
            return Severity::Warning;
        }
    } // namespace

    /// The command line: output contract, then the checks and the build context the
    /// configuration already carries for the stack analyzer, then the user's own arguments
    /// (which can override what precedes, e.g. `--disable=style`), then the file.
    std::vector<std::string>
    CppCheckToolImplementation::buildArguments(const ctrace::ProgramConfig& config,
                                               const std::string& file)
    {
        std::vector<std::string> args;
        if (config.output.sarif_format)
        {
            args.push_back("--output-format=sarif");
        }
        else
        {
            args.push_back(std::string("--template=") + kOutputTemplate);
        }
        // cppcheck enables only `error` by default (85 of 320 checks); the warning-class
        // checks are what a static analysis run is expected to report.
        args.push_back("--enable=warning,style,performance,portability");
        args.push_back("--inline-suppr");
        for (const std::string& dir : config.stack_analyzer.include_dirs)
        {
            if (!dir.empty())
            {
                args.push_back("-I" + dir);
            }
        }
        for (const std::string& macro : config.stack_analyzer.defines)
        {
            if (!macro.empty())
            {
                args.push_back("-D" + macro);
            }
        }
        const std::string& jobs = config.stack_analyzer.jobs;
        if (!jobs.empty() && std::all_of(jobs.begin(), jobs.end(),
                                         [](unsigned char ch) { return std::isdigit(ch) != 0; }))
        {
            args.push_back("-j");
            args.push_back(jobs);
        }
        appendToolArguments(args, config, "cppcheck");
        args.push_back(file);
        return args;
    }

    std::vector<Diagnostic> CppCheckToolImplementation::parseDiagnostics(const std::string& output)
    {
        // One line per finding, exactly as kOutputTemplate lays it out.
        static const std::regex linePattern(R"(^(.+?):(\d+):(\d+): (\w+): (.*) \[([^\]]+)\]$)");
        std::vector<Diagnostic> diagnostics;
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line))
        {
            std::smatch match;
            if (!std::regex_match(line, match, linePattern))
            {
                continue;
            }
            Diagnostic diagnostic;
            diagnostic.tool = "cppcheck";
            diagnostic.file = match[1];
            diagnostic.line = static_cast<unsigned>(std::stoul(match[2]));
            diagnostic.column = static_cast<unsigned>(std::stoul(match[3]));
            diagnostic.severity = severityOf(match[4]);
            diagnostic.message = match[5];
            diagnostic.ruleId = match[6];
            diagnostics.push_back(std::move(diagnostic));
        }
        return diagnostics;
    }

    void CppCheckToolImplementation::execute(const std::string& file,
                                             const ctrace::ProgramConfig& config,
                                             ToolOutput& output) const
    {
        coretrace::log(coretrace::Level::Info, "Running cppcheck on {}\n", file);
        const auto run = runExternalTool(config, *this, buildArguments(config, file), output);
        if (!run)
        {
            return;
        }
        if (config.output.sarif_format)
        {
            output.result(run->output);
            if (const auto parsed = diagnosticsFromSarifText(run->output, name()))
            {
                output.diagnostics(*parsed);
            }
            else
            {
                coretrace::log(coretrace::Level::Warn, coretrace::Module(name()),
                               "SARIF output could not be interpreted; findings are not counted\n");
            }
            return;
        }
        const std::vector<Diagnostic> diagnostics = parseDiagnostics(run->output);
        if (!diagnostics.empty())
        {
            output.result(renderLines(diagnostics));
        }
        output.diagnostics(diagnostics);
    }

    std::string CppCheckToolImplementation::name() const
    {
        return "cppcheck";
    }
} // namespace ctrace
