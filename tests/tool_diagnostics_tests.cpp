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

    // One SARIF log for the whole run: one run per tool, rules collected, stable fingerprints.
    {
        const std::vector<Diagnostic> items = {
            {"ctrace_stack_analyzer", "ResourceLifetime.DoubleRelease", "tests/double_free.c", 12,
             5, Severity::Error, "double release of ptr", "CWE-415"},
            {"cppcheck", "doubleFree", "tests/double_free.c", 12, 5, Severity::Error,
             "Memory pointed to by 'ptr' is freed twice.", ""},
            {"cppcheck", "unreadVariable", "tests/dead_code.cc", 4, 11, Severity::Warning,
             "Variable 'y' is assigned a value that is never used.", ""},
            {"cppcheck", "unreadVariable", "tests/dead_code.cc", 14, 11, Severity::Warning,
             "Variable 'y' is assigned a value that is never used.", ""},
            {"tscancode", "", "", 0, 0, Severity::Info, "no location, no rule", ""},
        };
        const nlohmann::json log = renderSarif(items);
        report.expect(log.value("version", "") == "2.1.0" && log.contains("$schema") &&
                          log["runs"].is_array() && log["runs"].size() == 3,
                      "renderSarif: a 2.1.0 log with one run per tool");
        if (log["runs"].size() == 3)
        {
            const auto& runs = log["runs"];
            report.expect(runs[0]["tool"]["driver"]["name"] == "cppcheck" &&
                              runs[1]["tool"]["driver"]["name"] == "ctrace_stack_analyzer" &&
                              runs[2]["tool"]["driver"]["name"] == "tscancode",
                          "renderSarif: runs are ordered by tool name");
            const auto& cppcheck = runs[0];
            report.expect(cppcheck["results"].size() == 3 &&
                              cppcheck["tool"]["driver"]["rules"].size() == 2,
                          "renderSarif: every diagnostic is a result; rules are listed once");
            const auto& first = cppcheck["results"][0];
            report.expect(
                first["ruleId"] == "doubleFree" && first["level"] == "error" &&
                    first["message"]["text"] == "Memory pointed to by 'ptr' is freed twice." &&
                    first["locations"][0]["physicalLocation"]["artifactLocation"]["uri"] ==
                        "tests/double_free.c" &&
                    first["locations"][0]["physicalLocation"]["region"]["startLine"] == 12 &&
                    first["locations"][0]["physicalLocation"]["region"]["startColumn"] == 5,
                "renderSarif: rule, level, message and location are mapped");
            report.expect(cppcheck["results"][1]["level"] == "warning" &&
                              runs[2]["results"][0]["level"] == "note",
                          "renderSarif: warning and info map to the SARIF levels");
            const auto& analyzer = runs[1]["results"][0];
            report.expect(analyzer["properties"]["cwe"] == "CWE-415",
                          "renderSarif: the CWE travels as a property");
            const std::string fp1 = cppcheck["results"][1]["partialFingerprints"]["coretrace/v1"];
            const std::string fp2 = cppcheck["results"][2]["partialFingerprints"]["coretrace/v1"];
            const std::string fp0 = cppcheck["results"][0]["partialFingerprints"]["coretrace/v1"];
            report.expect(
                !fp1.empty() && fp1 == fp2 && fp0 != fp1,
                "renderSarif: fingerprints ignore the line and depend on rule and message");
            report.expect(
                fp1 == renderSarif(
                           items)["runs"][0]["results"][1]["partialFingerprints"]["coretrace/v1"]
                           .get<std::string>(),
                "renderSarif: fingerprints are stable across renderings");
            const auto& bare = runs[2]["results"][0];
            report.expect(!bare.contains("ruleId") && !bare.contains("locations") &&
                              runs[2]["tool"]["driver"]["rules"].empty(),
                          "renderSarif: no rule and no location are left out, not zeroed");
        }
        report.expect(renderSarif({})["runs"].is_array() && renderSarif({})["runs"].empty(),
                      "renderSarif: nothing to report is an empty runs array");
    }

    if (report.failures == 0)
    {
        std::cout << "tool_diagnostics_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " diagnostic model check(s) failed\n";
    return 1;
}
