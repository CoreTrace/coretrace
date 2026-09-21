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
        const auto text = CppCheckToolImplementation::buildArguments(configWithSarif(false), "a.c");
        report.expect(!contains(text, "--output-format=sarif"),
                      "cppcheck: no SARIF flag without the request");
        // The template is the parsing contract: the same on every cppcheck version.
        report.expect(contains(text, std::string("--template=") +
                                         CppCheckToolImplementation::kOutputTemplate) &&
                          text.back() == "a.c",
                      "cppcheck: text runs use the explicit output template, file last");
    }
    {
        const auto args =
            FlawfinderToolImplementation::buildArguments(configWithSarif(true), "a.c");
        report.expect(contains(args, "--sarif") && args.back() == "a.c",
                      "flawfinder: SARIF request is forwarded and the file comes last");
        report.expect(
            contains(FlawfinderToolImplementation::buildArguments(configWithSarif(false), "a.c"),
                     "--sarif"),
            "flawfinder: the structured output is always requested; CoreTrace renders the text");
        report.expect(std::none_of(args.begin(), args.end(), [](const std::string& arg)
                                   { return arg.find(".py") != std::string::npos; }),
                      "flawfinder: no script path in the arguments");
    }

    // Tools are named, not located: PATH resolves them unless the configuration says otherwise.
    {
        ctrace::ProgramConfig config;
        report.expect(toolCommand(config, CppCheckToolImplementation()) == "cppcheck" &&
                          toolCommand(config, IkosToolImplementation()) == "ikos" &&
                          toolCommand(config, FlawfinderToolImplementation()) == "flawfinder" &&
                          toolCommand(config, TscancodeToolImplementation()) == "tscancode",
                      "default command is the tool name, resolved through PATH");
        report.expect(toolCommand(config, CppCheckToolImplementation()).find('/') ==
                          std::string::npos,
                      "default command carries no path separator");

        config.tools.paths["cppcheck"] = "/opt/homebrew/bin/cppcheck";
        report.expect(toolCommand(config, CppCheckToolImplementation()) ==
                          "/opt/homebrew/bin/cppcheck",
                      "a configured path overrides the lookup");
        report.expect(toolCommand(config, IkosToolImplementation()) == "ikos",
                      "an override for one tool leaves the others alone");
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
