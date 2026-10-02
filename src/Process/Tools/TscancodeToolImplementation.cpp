// SPDX-License-Identifier: Apache-2.0
#include "Process/Tools/AnalysisTools.hpp"

#include <coretrace/logger.hpp>

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace ctrace
{

    std::vector<std::string>
    TscancodeToolImplementation::buildArguments(const ProgramConfig& config,
                                                const std::filesystem::path& fileList)
    {
        std::vector<std::string> args = {"--enable=all"};
        appendToolArguments(args, config, "tscancode");
        args.push_back("--file-list=" + fileList.string());
        return args;
    }

    void TscancodeToolImplementation::executeBatch(const std::vector<std::string>& files,
                                                   const ProgramConfig& config,
                                                   ToolOutput& output) const
    {
        const std::string inputs = ctrace_tools::strings::joinByComma(files);
        coretrace::log(coretrace::Level::Info, "Running tscancode on {}\n", inputs);
        const RunDirectory runDirectory("ctrace-tscancode");
        const std::filesystem::path fileList = runDirectory.path() / "files.txt";
        writeFileList(fileList, files);
        const auto run = runExternalTool(config, *this, buildArguments(config, fileList), output,
                                         {0}, toolTimeout(config, name(), files.size()));
        if (!run)
        {
            return;
        }
        coretrace::log(coretrace::Level::Debug, "Finished tscancode on {}\n", inputs);
        const std::vector<Diagnostic> diagnostics = parseDiagnostics(run->output);
        // In SARIF mode the merged document is the output; text lines would pollute it.
        if (!config.output.sarif_format && !diagnostics.empty())
        {
            output.result(renderLines(diagnostics));
        }
        output.diagnostics(diagnostics);
    }

    std::string TscancodeToolImplementation::name() const
    {
        return "tscancode";
    }

    namespace
    {
        const std::regex& tscancodeLinePattern()
        {
            static const std::regex pattern(R"(\[(.*):(\d+)\]: \((\w+)\) (.*))");
            return pattern;
        }

        [[nodiscard]] Severity severityOf(std::string word)
        {
            std::transform(word.begin(), word.end(), word.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if (word == "error")
            {
                return Severity::Error;
            }
            if (word == "information")
            {
                return Severity::Info;
            }
            return Severity::Warning;
        }
    } // namespace

    std::vector<Diagnostic> TscancodeToolImplementation::parseDiagnostics(const std::string& output)
    {
        std::vector<Diagnostic> diagnostics;
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line))
        {
            std::smatch match;
            if (!std::regex_match(line, match, tscancodeLinePattern()))
            {
                continue;
            }
            Diagnostic diagnostic;
            diagnostic.tool = "tscancode";
            diagnostic.file = match[1];
            diagnostic.line = static_cast<unsigned>(std::stoul(match[2]));
            diagnostic.severity = severityOf(match[3]);
            diagnostic.message = match[4];
            diagnostics.push_back(std::move(diagnostic));
        }
        return diagnostics;
    }

} // namespace ctrace
