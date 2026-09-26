// SPDX-License-Identifier: Apache-2.0
//
// One run over C and Python sources with the tools of both languages: a single SARIF log with
// one run per tool, where every finding lies in a file of its tool's language.
//
// Usage: ctrace_mixed_language_tests <ctrace> <repository root> <config> <work directory>
#include "Process/ProcessFactory.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using json = nlohmann::json;

    struct TestReport
    {
        int failures = 0;

        void expect(bool condition, const std::string& message)
        {
            if (condition)
            {
                std::cout << "[PASS] " << message << "\n";
                return;
            }
            ++failures;
            std::cerr << "[FAIL] " << message << "\n";
        }
    };

    bool endsWith(const std::string& text, const std::string& suffix)
    {
        return text.size() >= suffix.size() &&
               text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    /// The file of every result, by the tool whose run holds it.
    std::map<std::string, std::vector<std::string>> resultFilesByTool(const json& log)
    {
        std::map<std::string, std::vector<std::string>> files;
        for (const json& run : log.value("runs", json::array()))
        {
            const std::string tool =
                run.value("tool", json::object()).value("driver", json::object()).value("name", "");
            std::vector<std::string>& uris = files[tool];
            for (const json& result : run.value("results", json::array()))
            {
                for (const json& location : result.value("locations", json::array()))
                {
                    uris.push_back(location.value("physicalLocation", json::object())
                                       .value("artifactLocation", json::object())
                                       .value("uri", ""));
                }
            }
        }
        return files;
    }
} // namespace

int main(int argc, char* argv[])
{
    if (argc != 5)
    {
        std::cerr << "Usage: ctrace_mixed_language_tests <ctrace> <repository root> <config> "
                     "<work directory>\n";
        return 2;
    }
    const fs::path root = argv[2];
    const fs::path sarifPath = fs::path(argv[4]) / "mixed-language.sarif";
    fs::remove(sarifPath);
    TestReport report;

    const std::string inputs = (root / "tests/double_free.c").string() + "," +
                               (root / "tests/python/project/app/main.py").string();
    auto process = ProcessFactory::createProcess(
        argv[1], {"--sarif-format", "--fail-on", "none", "--config", argv[3], "--invoke",
                  "cppcheck,ctrace_stack_analyzer,coretrace-python-analyzer", "--input", inputs,
                  "--report-file", sarifPath.string()});
    const ProcessResult run = process->execute();
    report.expect(run.succeeded(), "the run over C and Python completes (exit " +
                                       std::to_string(run.exitCode) + ")");

    std::ifstream in(sarifPath);
    const json log = json::parse(in, nullptr, /*allow_exceptions=*/false);
    report.expect(log.is_object() && log.contains("runs"), "the report file is one SARIF log");
    const auto files = resultFilesByTool(log.is_object() ? log : json::object());

    std::set<std::string> tools;
    for (const auto& [tool, _] : files)
    {
        tools.insert(tool);
    }
    report.expect((tools == std::set<std::string>{"cppcheck", "ctrace_stack_analyzer",
                                                  "coretrace-python-analyzer"}),
                  "the log holds one run per tool");

    // A finding both C tools report is merged into one result: count them together.
    std::vector<std::string> cFiles;
    for (const char* tool : {"cppcheck", "ctrace_stack_analyzer"})
    {
        if (const auto found = files.find(tool); found != files.end())
        {
            cFiles.insert(cFiles.end(), found->second.begin(), found->second.end());
        }
    }
    report.expect(!cFiles.empty() &&
                      std::all_of(cFiles.begin(), cFiles.end(), [](const std::string& uri)
                                  { return endsWith(uri, "double_free.c"); }),
                  "the C tools report on the C source only");

    const auto python = files.find("coretrace-python-analyzer");
    const std::vector<std::string> pythonFiles =
        python != files.end() ? python->second : std::vector<std::string>{};
    report.expect(std::any_of(pythonFiles.begin(), pythonFiles.end(),
                              [](const std::string& uri) { return endsWith(uri, "app/main.py"); }),
                  "the Python analyzer reports on the Python input");
    report.expect(std::none_of(pythonFiles.begin(), pythonFiles.end(),
                               [](const std::string& uri) { return endsWith(uri, ".c"); }),
                  "the Python analyzer never reports on the C source");

    fs::remove(sarifPath);
    if (report.failures == 0)
    {
        std::cout << "mixed_language_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " mixed language check(s) failed\n";
    return 1;
}
