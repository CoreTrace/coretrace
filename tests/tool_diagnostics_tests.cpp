// SPDX-License-Identifier: Apache-2.0
//
// The unified diagnostic model: every tool's findings are normalized to ctrace::Diagnostic so
// that counting, gating and merging work the same whatever produced them.
#include "Process/Tools/AnalysisTools.hpp"
#include "Process/Tools/Diagnostic.hpp"
#include "Process/Tools/Sarif.hpp"

#include <nlohmann/json.hpp>

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
} // namespace

int main()
{
    using namespace ctrace;
    TestReport report;

    // Summaries are derived from the diagnostics, never reported on their own.
    {
        const std::vector<Diagnostic> items = {
            {"t", "r1", "a.c", 1, 1, Severity::Error, "boom", ""},
            {"t", "r2", "a.c", 2, 1, Severity::Warning, "hmm", ""},
            {"t", "r3", "a.c", 3, 1, Severity::Info, "fyi", ""},
            {"t", "r4", "a.c", 4, 1, Severity::Error, "boom again", ""},
        };
        const DiagnosticSummary summary = summarize(items);
        report.expect(summary.info == 1 && summary.warning == 1 && summary.error == 2,
                      "summarize counts each severity");
        report.expect(summarize({}).error == 0 && summarize({}).warning == 0,
                      "summarize of nothing is all zeros");
    }

    // One human-readable line per diagnostic, in the gcc/cppcheck style IDEs already parse.
    {
        const Diagnostic item{"cppcheck",
                              "doubleFree",
                              "tests/double_free.c",
                              12,
                              5,
                              Severity::Error,
                              "Memory pointed to by 'ptr' is freed twice.",
                              ""};
        report.expect(renderLine(item) == "tests/double_free.c:12:5: error: Memory pointed to by "
                                          "'ptr' is freed twice. [cppcheck/doubleFree]",
                      "renderLine: file:line:col: severity: message [tool/rule]");
        const Diagnostic noRule{"ikos", "", "x.c", 3, 0, Severity::Warning, "msg", ""};
        report.expect(renderLine(noRule) == "x.c:3: warning: msg [ikos]",
                      "renderLine: no column and no rule are omitted");
    }

    // cppcheck runs with an explicit --template so its output is a contract, not a default.
    {
        const std::string output =
            "Checking tests/double_free.c ...\n"
            "tests/double_free.c:12:5: error: Memory pointed to by 'ptr' is freed twice. "
            "[doubleFree]\n"
            "tests/a.cc:4:11: style: Variable 'y' is assigned a value that is never used. "
            "[unreadVariable]\n"
            "nofile:0:0: information: Active checkers: 167/856 [checkersReport]\n"
            "1/2 files checked 50% done\n";
        const auto items = CppCheckToolImplementation::parseDiagnostics(output);
        report.expect(items.size() == 3, "cppcheck: only diagnostic lines are parsed (got " +
                                             std::to_string(items.size()) + ")");
        if (items.size() == 3)
        {
            report.expect(items[0].tool == "cppcheck" && items[0].ruleId == "doubleFree" &&
                              items[0].file == "tests/double_free.c" && items[0].line == 12 &&
                              items[0].column == 5 && items[0].severity == Severity::Error &&
                              items[0].message == "Memory pointed to by 'ptr' is freed twice.",
                          "cppcheck: every field of the template line is mapped");
            report.expect(items[1].severity == Severity::Warning, "cppcheck: style is a warning");
            report.expect(items[2].severity == Severity::Info, "cppcheck: information is info");
        }
        report.expect(CppCheckToolImplementation::parseDiagnostics("").empty(),
                      "cppcheck: empty output has no diagnostics");
    }

    // A generic SARIF reader covers every tool that writes SARIF itself (flawfinder today).
    {
        const nlohmann::json log = nlohmann::json::parse(R"({
          "version": "2.1.0",
          "runs": [{
            "tool": {"driver": {"name": "Flawfinder"}},
            "results": [
              {"ruleId": "FF1001", "level": "error",
               "message": {"text": "buffer/strcpy:Does not check for buffer overflows (CWE-120)."},
               "locations": [{"physicalLocation": {
                  "artifactLocation": {"uri": "src/x.cc"},
                  "region": {"startLine": 11, "startColumn": 5}}}],
               "properties": {"cwe": "CWE-120"}},
              {"ruleId": "FF1016", "level": "note",
               "message": {"text": "format/printf:If format strings can be influenced."},
               "locations": [{"physicalLocation": {
                  "artifactLocation": {"uri": "src/x.cc"},
                  "region": {"startLine": 8}}}]},
              {"ruleId": "FF1", "message": {"text": "no level defaults to warning"},
               "locations": []}
            ]
          }]
        })");
        const auto parsed = diagnosticsFromSarif(log, "flawfinder");
        report.expect(parsed.has_value() && parsed->size() == 3,
                      "sarif: every result becomes a diagnostic");
        const std::vector<Diagnostic> items = parsed.value_or(std::vector<Diagnostic>{});
        if (items.size() == 3)
        {
            report.expect(items[0].tool == "flawfinder" && items[0].ruleId == "FF1001" &&
                              items[0].file == "src/x.cc" && items[0].line == 11 &&
                              items[0].column == 5 && items[0].severity == Severity::Error &&
                              items[0].cwe == "CWE-120",
                          "sarif: location, level and CWE are mapped");
            report.expect(items[1].severity == Severity::Info && items[1].column == 0,
                          "sarif: note maps to info; missing column is 0");
            report.expect(items[2].severity == Severity::Warning && items[2].file.empty(),
                          "sarif: missing level defaults to warning; missing location is empty");
        }
        report.expect(!diagnosticsFromSarif(nlohmann::json::parse("{}"), "x").has_value(),
                      "sarif: a document without runs is not interpreted");
        report.expect(FlawfinderToolImplementation::parseDiagnostics("not json").has_value() ==
                          false,
                      "flawfinder: unparsable output is reported as not interpreted");
        const auto flaw = FlawfinderToolImplementation::parseDiagnostics(log.dump());
        report.expect(flaw.has_value() && flaw->size() == 3 && flaw->front().tool == "flawfinder",
                      "flawfinder: its SARIF output feeds the model");
    }

    // tscancode keeps its text parser, now producing the model instead of a private SARIF.
    {
        const std::string output = "[src/main.c:12]: (error) Memory leak: buffer\n"
                                   "[src/util.c:3]: (Warning) Unused variable\n"
                                   "[src/util.c:4]: (Information) fyi\n"
                                   "not a diagnostic line\n";
        const auto items = TscancodeToolImplementation::parseDiagnostics(output);
        report.expect(items.size() == 3, "tscancode: only diagnostic lines become diagnostics");
        if (items.size() == 3)
        {
            report.expect(items[0].tool == "tscancode" && items[0].file == "src/main.c" &&
                              items[0].line == 12 && items[0].severity == Severity::Error &&
                              items[0].message == "Memory leak: buffer" && items[0].ruleId.empty(),
                          "tscancode: file, line, severity and message are mapped");
            report.expect(items[1].severity == Severity::Warning &&
                              items[2].severity == Severity::Info,
                          "tscancode: Warning and Information map case-insensitively");
        }
    }

    if (report.failures == 0)
    {
        std::cout << "tool_diagnostics_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " diagnostic model check(s) failed\n";
    return 1;
}
