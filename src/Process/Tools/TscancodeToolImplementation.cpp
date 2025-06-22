#include "Process/Tools/AnalysisTools.hpp"

#include <unordered_map>
#include <string_view>

namespace ctrace {

void TscancodeToolImplementation::execute(const std::string& file, ProgramConfig config) const
{
    ctrace::Thread::Output::cout("\033[32mRunning tscancode on " + file + "\033[0m");

    bool has_sarif_format = config.global.hasSarifFormat;
    std::string src_file = file;

    try
    {
        std::vector<std::string> argsProcess;
        argsProcess.push_back("--enable=all");
        argsProcess.push_back(src_file);

        auto process = ProcessFactory::createProcess("./tscancode/src/tscancode/trunk/tscancode", argsProcess);
        process->execute();
        ctrace::Thread::Output::cout(process->logOutput);
        ctrace::Thread::Output::cout("Finished tscancode on " + file);

        if (has_sarif_format) {
            ctrace::Thread::Output::cout(sarifFormat(process->logOutput, "ccoretrace-sarif-tscancode.json").dump());
        }

    }
    catch (const std::exception& e)
    {
        ctrace::Thread::Output::cerr("Error: " + std::string(e.what()));
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
        {"Warning", "warning"},
        {"Information", "note"},
        {"Error", "error"}
    };
    static const std::unordered_map<std::string_view, std::string_view> severity_map(mappings, mappings + std::size(mappings));

    if (const auto it = severity_map.find(severity); it != severity_map.end())
    {
        return it->second;
    }
    return "none";
}

json TscancodeToolImplementation::sarifFormat(const std::string &buffer, const std::string &outputFile) const
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

    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_match(line, match, diagnostic_regex)) {
            std::string filePath = match[1];
            int lineNumber = std::stoi(match[2]);
            std::string severity = match[3];
            std::string message = match[4];

            std::string ruleId = "coretrace." + severity;

            if (ruleMap.find(ruleId) == ruleMap.end()) {
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
            result["locations"] = {{
                {"physicalLocation", {
                    {"artifactLocation", {{"uri", filePath}}},
                    {"region", {{"startLine", lineNumber}}}
                }}
            }};
            run["results"].push_back(result);
        }
    }

    sarif["runs"].push_back(run);

    std::ofstream out(outputFile);
    out << sarif.dump(4);
    return sarif;
}

} // namespace ctrace
