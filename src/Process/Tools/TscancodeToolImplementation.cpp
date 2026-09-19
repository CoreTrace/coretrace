// SPDX-License-Identifier: Apache-2.0
#include "Process/Tools/AnalysisTools.hpp"

#include <coretrace/logger.hpp>

#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace ctrace
{

    void TscancodeToolImplementation::execute(const std::string& file, const ProgramConfig& config,
                                              ToolOutput& output) const
    {
        coretrace::log(coretrace::Level::Info, "Running tscancode on {}\n", file);

        bool has_sarif_format = config.output.sarif_format;
        std::string src_file = file;

        try
        {
            std::vector<std::string> argsProcess;
            argsProcess.push_back("--enable=all");
            argsProcess.push_back(src_file);

            auto process = ProcessFactory::createProcess(
                "./tscancode/src/tscancode/trunk/tscancode", argsProcess);
            const ProcessResult run = process->execute();
            output.result(run.output);
            coretrace::log(coretrace::Level::Debug, "Finished tscancode on {}\n", file);
            if (!run.succeeded())
            {
                output.error(run.describeFailure(name()));
            }

            if (has_sarif_format)
            {
                output.result(sarifFormat(run.output, "ccoretrace-sarif-tscancode.json").dump());
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

    std::string_view TscancodeToolImplementation::severityToLevel(const std::string& severity) const
    {
        static constexpr std::pair<std::string_view, std::string_view> mappings[] = {
            {"Warning", "warning"}, {"Information", "note"}, {"Error", "error"}};
        static const std::unordered_map<std::string_view, std::string_view> severity_map(
            mappings, mappings + std::size(mappings));

        if (const auto it = severity_map.find(severity); it != severity_map.end())
        {
            return it->second;
        }
        return "none";
    }

    json TscancodeToolImplementation::sarifFormat(const std::string& buffer,
                                                  const std::string& outputFile) const
    {
        std::regex diagnostic_regex(R"(\[(.*):(\d+)\]: \((\w+)\) (.*))");

        json sarif;
        sarif["version"] = "2.1.0";
        sarif["$schema"] = "https://json.schemastore.org/sarif-2.1.0.json";
        sarif["runs"] = json::array();

        json run;
        run["tool"]["driver"]["name"] = "coretrace";
        run["tool"]["driver"]["version"] = "1.0.0";
        run["tool"]["driver"]["informationUri"] = "https://coretrace.fr/";
        run["tool"]["driver"]["rules"] = json::array();
        run["results"] = json::array();

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
                    json rule;
                    rule["id"] = ruleId;
                    rule["name"] = severity;
                    rule["shortDescription"]["text"] = severity + " reported by coretrace";
                    run["tool"]["driver"]["rules"].push_back(rule);
                    ruleMap[ruleId] = ruleCounter++;
                }

                json result;
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

        std::ofstream out(outputFile);
        out << sarif.dump(4);
        return sarif;
    }

} // namespace ctrace
