// SPDX-License-Identifier: Apache-2.0
#include "Process/Tools/AnalysisTools.hpp"

#include <coretrace/logger.hpp>

#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace ctrace
{

    std::vector<std::string> TscancodeToolImplementation::buildArguments(const ProgramConfig&,
                                                                         const std::string& file)
    {
        return {"--enable=all", file};
    }

    void TscancodeToolImplementation::execute(const std::string& file, const ProgramConfig& config,
                                              ToolOutput& output) const
    {
        coretrace::log(coretrace::Level::Info, "Running tscancode on {}\n", file);
        const auto run = runExternalTool(config, *this, buildArguments(config, file), output);
        if (!run)
        {
            return;
        }
        coretrace::log(coretrace::Level::Debug, "Finished tscancode on {}\n", file);
        const std::vector<Diagnostic> diagnostics = parseDiagnostics(run->output);
        if (config.output.sarif_format)
        {
            output.result(sarifFormat(run->output).dump());
        }
        else if (!diagnostics.empty())
        {
            output.result(renderLines(diagnostics));
        }
        output.diagnostics(diagnostics);
    }

    std::string TscancodeToolImplementation::name() const
    {
        return "tscancode";
    }

    /// tscancode spells severities in lower case in its text output and capitalized in its
    /// documentation, so the mapping is case-insensitive.
    std::string_view TscancodeToolImplementation::severityToLevel(const std::string& severity) const
    {
        static constexpr std::pair<std::string_view, std::string_view> mappings[] = {
            {"warning", "warning"}, {"information", "note"}, {"error", "error"}};
        static const std::unordered_map<std::string_view, std::string_view> severity_map(
            mappings, mappings + std::size(mappings));

        std::string lowered = severity;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

        if (const auto it = severity_map.find(lowered); it != severity_map.end())
        {
            return it->second;
        }
        return "none";
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

    nlohmann::json TscancodeToolImplementation::sarifFormat(const std::string& buffer) const
    {
        const std::regex& diagnostic_regex = tscancodeLinePattern();

        nlohmann::json sarif;
        sarif["version"] = "2.1.0";
        sarif["$schema"] = "https://json.schemastore.org/sarif-2.1.0.json";
        sarif["runs"] = nlohmann::json::array();

        nlohmann::json run;
        run["tool"]["driver"]["name"] = "coretrace";
        run["tool"]["driver"]["version"] = "1.0.0";
        run["tool"]["driver"]["informationUri"] = "https://coretrace.fr/";
        run["tool"]["driver"]["rules"] = nlohmann::json::array();
        run["results"] = nlohmann::json::array();

        std::map<std::string, int> ruleMap;
        int ruleCounter = 0;

        std::istringstream stream(buffer);
        std::string line;

        while (std::getline(stream, line))
        {
            std::smatch match;
            if (std::regex_match(line, match, diagnostic_regex))
            {
                std::string filePath = match[1];
                int lineNumber = std::stoi(match[2]);
                std::string severity = match[3];
                std::string message = match[4];

                std::string ruleId = "coretrace." + severity;

                if (ruleMap.find(ruleId) == ruleMap.end())
                {
                    nlohmann::json rule;
                    rule["id"] = ruleId;
                    rule["name"] = severity;
                    rule["shortDescription"]["text"] = severity + " reported by coretrace";
                    run["tool"]["driver"]["rules"].push_back(rule);
                    ruleMap[ruleId] = ruleCounter++;
                }

                nlohmann::json result;
                result["ruleId"] = ruleId;
                result["level"] = severityToLevel(severity);
                result["message"]["text"] = message;
                result["locations"] = {{{"physicalLocation",
                                         {{"artifactLocation", {{"uri", filePath}}},
                                          {"region", {{"startLine", lineNumber}}}}}}};
                run["results"].push_back(result);
            }
        }

        sarif["runs"].push_back(run);
        return sarif;
    }

} // namespace ctrace
