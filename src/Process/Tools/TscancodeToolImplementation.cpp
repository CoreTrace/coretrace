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

        const bool has_sarif_format = config.output.sarif_format;

        try
        {
            auto process = ProcessFactory::createProcess(
                "./tscancode/src/tscancode/trunk/tscancode", buildArguments(config, file));
            const ProcessResult run = process->execute();
            output.result(run.output);
            coretrace::log(coretrace::Level::Debug, "Finished tscancode on {}\n", file);
            if (!run.succeeded())
            {
                output.error(run.describeFailure(name()));
            }

            if (has_sarif_format)
            {
                output.result(sarifFormat(run.output).dump());
            }
        }
        catch (const std::exception& e)
        {
            output.error("Error: " + std::string(e.what()));
            return;
        }
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

    nlohmann::json TscancodeToolImplementation::sarifFormat(const std::string& buffer) const
    {
        std::regex diagnostic_regex(R"(\[(.*):(\d+)\]: \((\w+)\) (.*))");

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
