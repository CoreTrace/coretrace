// SPDX-License-Identifier: Apache-2.0
//
// Prints where the results of a SARIF log are: one `<tool>\t<file>` line per location, related
// locations included, sorted and without repeats, and `<tool>\t(none)` for a result that has no
// location. Tests compare that list with the spelling they expect, instead of searching the
// text, where an extra location or an unexpected spelling would go unnoticed.
//
// Usage: ctrace_sarif_locations <SARIF file>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <set>
#include <string>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: ctrace_sarif_locations <SARIF file>\n";
        return 2;
    }
    std::ifstream file(argv[1]);
    const nlohmann::json log = nlohmann::json::parse(file, nullptr, false);
    if (log.is_discarded() || !log.contains("runs") || !log["runs"].is_array())
    {
        std::cerr << argv[1] << " is not a SARIF log\n";
        return 2;
    }

    const nlohmann::json::json_pointer uri("/physicalLocation/artifactLocation/uri");
    std::set<std::string> lines;
    for (const nlohmann::json& run : log["runs"])
    {
        const std::string tool =
            run.value(nlohmann::json::json_pointer("/tool/driver/name"), std::string("(no tool)"));
        for (const nlohmann::json& result : run.value("results", nlohmann::json::array()))
        {
            const nlohmann::json locations = result.value("locations", nlohmann::json::array());
            if (locations.empty())
            {
                lines.insert(tool + "\t(none)");
            }
            for (const char* field : {"locations", "relatedLocations"})
            {
                for (const nlohmann::json& location : result.value(field, nlohmann::json::array()))
                {
                    lines.insert(tool + "\t" + location.value(uri, std::string("(no uri)")));
                }
            }
        }
    }
    for (const std::string& line : lines)
    {
        std::cout << line << "\n";
    }
    return 0;
}
