// SPDX-License-Identifier: Apache-2.0
#include "Process/Tools/AnalysisTools.hpp"
#include <coretrace/logger.hpp>

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

    /// The command line: the output template CoreTrace parses, then the checks and the build context the
    /// configuration carries, then the user's own arguments (which can override what precedes,
    /// e.g. `--disable=style`), then the list of files.
    std::vector<std::string>
    CppCheckToolImplementation::buildArguments(const ctrace::ProgramConfig& config,
                                               const std::filesystem::path& fileList)
    {
        std::vector<std::string> args;
        args.push_back(std::string("--template=") + kOutputTemplate);
        // cppcheck enables only `error` by default (85 of 320 checks); the warning-class
        // checks are what a static analysis run is expected to report.
        args.push_back("--enable=warning,style,performance,portability");
        args.push_back("--inline-suppr");
        appendBuildContext(args, config);
        // tools.cppcheck.jobs only: without it cppcheck keeps its own default.
        if (config.tools.cppcheck_jobs != 0)
        {
            args.push_back("-j");
            args.push_back(std::to_string(config.tools.cppcheck_jobs));
        }
        appendToolArguments(args, config, "cppcheck");
        args.push_back("--file-list=" + fileList.string());
        return args;
    }

    std::vector<Diagnostic> CppCheckToolImplementation::parseDiagnostics(const std::string& output)
    {
        // One line per finding, exactly as kOutputTemplate lays it out.
        static const std::regex linePattern(
            R"(^(.+?):(\d+):(\d+): (\w+): (.*) \[([^\]]+)\] \[CWE-(\d+)\]$)");
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
            if (match[7] != "0")
            {
                diagnostic.cwe = "CWE-" + match[7].str();
            }
            diagnostics.push_back(std::move(diagnostic));
        }
        return diagnostics;
    }

    void CppCheckToolImplementation::executeBatch(const std::vector<std::string>& files,
                                                  const ctrace::ProgramConfig& config,
                                                  ToolOutput& output) const
    {
        coretrace::log(coretrace::Level::Info, "Running cppcheck on {}\n",
                       ctrace_tools::strings::joinByComma(files));
        const RunDirectory runDirectory("ctrace-cppcheck");
        const std::filesystem::path fileList = runDirectory.path() / "files.txt";
        writeFileList(fileList, files);
        const auto run = runExternalTool(config, *this, buildArguments(config, fileList), output,
                                         {0}, toolTimeout(config, name(), files.size()));
        if (!run)
        {
            return;
        }
        const std::vector<Diagnostic> diagnostics = parseDiagnostics(run->output);
        // In SARIF mode the merged document is the output; text lines would pollute it.
        if (!config.output.sarif_format && !diagnostics.empty())
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
