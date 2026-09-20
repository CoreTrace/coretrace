// SPDX-License-Identifier: Apache-2.0
//
// Each tool reports its own identity and builds its command line from the configuration.
#include "Process/Tools/AnalysisTools.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace
{
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

    bool contains(const std::vector<std::string>& args, const std::string& value)
    {
        return std::find(args.begin(), args.end(), value) != args.end();
    }

    ctrace::ProgramConfig configWithSarif(bool sarif)
    {
        ctrace::ProgramConfig config;
        config.output.sarif_format = sarif;
        config.files.entry_points = {"main"};
        return config;
    }
} // namespace

int main()
{
    using namespace ctrace;
    TestReport report;

    // A tool's name identifies it in logs, diagnostics summaries and the server response.
    report.expect(CppCheckToolImplementation().name() == "cppcheck",
                  "cppcheck reports its own name");
    report.expect(IkosToolImplementation().name() == "ikos", "ikos reports its own name");
    report.expect(FlawfinderToolImplementation().name() == "flawfinder",
                  "flawfinder reports its own name");
    report.expect(TscancodeToolImplementation().name() == "tscancode",
                  "tscancode reports its own name");

    // --sarif-format asks each tool for its structured output, never for text.
    {
        const auto args = IkosToolImplementation::buildArguments(configWithSarif(true), "a.c");
        report.expect(contains(args, "--format=json") && !contains(args, "--format=text"),
                      "ikos: a SARIF request asks for the structured format");
        report.expect(contains(args, "a.c"), "ikos: the source file is passed");
    }
    {
        const auto args = IkosToolImplementation::buildArguments(configWithSarif(false), "a.c");
        report.expect(contains(args, "--format=text") && !contains(args, "--format=json"),
                      "ikos: without SARIF the text format is used");
    }
    {
        const auto args = CppCheckToolImplementation::buildArguments(configWithSarif(true), "a.c");
        report.expect(contains(args, "--output-format=sarif") && contains(args, "a.c"),
                      "cppcheck: SARIF request is forwarded");
        report.expect(
            !contains(CppCheckToolImplementation::buildArguments(configWithSarif(false), "a.c"),
                      "--output-format=sarif"),
            "cppcheck: no SARIF flag without the request");
    }
    {
        const auto args =
            FlawfinderToolImplementation::buildArguments(configWithSarif(true), "a.c");
        report.expect(contains(args, "--sarif") && args.back() == "a.c",
                      "flawfinder: SARIF request is forwarded and the file comes last");
    }
    {
        const auto args =
            TscancodeToolImplementation::buildArguments(configWithSarif(false), "a.c");
        report.expect(contains(args, "--enable=all") && args.back() == "a.c",
                      "tscancode: enables all checks and passes the file");
    }

    // The SARIF conversion of tscancode output is a pure transformation.
    {
        const TscancodeToolImplementation tool;
        const std::string toolOutput = "[src/main.c:12]: (error) Memory leak: buffer\n"
                                       "[src/util.c:3]: (Warning) Unused variable\n"
                                       "not a diagnostic line\n";
        const nlohmann::json sarif = tool.sarifFormat(toolOutput);
        const auto& results = sarif["runs"][0]["results"];
        report.expect(sarif.value("version", "") == "2.1.0" && results.size() == 2,
                      "tscancode SARIF: only diagnostic lines become results");
        if (results.size() == 2)
        {
            report.expect(
                results[0]["level"] == "error" &&
                    results[0]["locations"][0]["physicalLocation"]["artifactLocation"]["uri"] ==
                        "src/main.c" &&
                    results[0]["locations"][0]["physicalLocation"]["region"]["startLine"] == 12,
                "tscancode SARIF: severity and location are mapped");
            report.expect(results[1]["level"] == "warning",
                          "tscancode SARIF: Warning maps to the SARIF warning level");
        }
    }

    if (report.failures == 0)
    {
        std::cout << "tool_arguments_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " tool argument check(s) failed\n";
    return 1;
}
