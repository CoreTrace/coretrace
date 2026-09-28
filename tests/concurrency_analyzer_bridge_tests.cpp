// SPDX-License-Identifier: Apache-2.0
//
// Compatibility gate between coretrace and the pinned coretrace-concurrency-analyzer library:
// each field of the analyzer's Diagnostic in CoreTrace's model, and the rule names the
// configuration accepts.
#include "Process/Tools/ConcurrencyAnalyzerBridge.hpp"
#include "Process/Tools/Diagnostic.hpp"
#include "Process/Tools/InputPaths.hpp"

#include <coretrace_concurrency_analysis.hpp>

#include <algorithm>
#include <filesystem>
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

    using ctrace::concurrency::ConfidenceLevel;
    using ctrace::concurrency::RuleId;

    ctrace::concurrency::Diagnostic raceFinding(const std::string& file)
    {
        ctrace::concurrency::Diagnostic finding;
        finding.id = "DataRaceGlobal-counter";
        finding.severity = ctrace::concurrency::Severity::Error;
        finding.ruleId = RuleId::DataRaceGlobal;
        finding.confidence = ConfidenceLevel::High;
        finding.taxonomies = {{"CWE", "362", "Race Condition"}};
        finding.location = {file, 14, 9, 14, 30, "worker"};
        finding.relatedLocations = {{"Conflicting access", {file, 14, 9, 14, 30, "worker"}}};
        finding.message = "unsynchronized concurrent access to global 'counter'";
        finding.notes = {{"no common recognized lock protects the conflicting accesses"}};
        return finding;
    }

    bool hasRule(const ctrace::concurrency::AnalysisOptions& options, RuleId rule)
    {
        return std::find(options.enabledRules.begin(), options.enabledRules.end(), rule) !=
               options.enabledRules.end();
    }
} // namespace

int main()
{
    TestReport report;
    const std::string tool = "coretrace-concurrency-analyzer";

    // Rules are named as on the analyzer's own command line; nothing means every rule.
    {
        std::string error;
        const auto all = ctrace::concurrencyRules({}, error);
        report.expect(all.has_value() && error.empty() &&
                          all->enabledRules ==
                              ctrace::concurrency::AnalysisOptions::allAvailable().enabledRules,
                      "rules: no name selects every rule the analyzer implements");

        const auto two =
            ctrace::concurrencyRules({"data-race", "missing-join", "data-race"}, error);
        report.expect(two.has_value() && two->enabledRules.size() == 2 &&
                          hasRule(*two, RuleId::DataRaceGlobal) &&
                          hasRule(*two, RuleId::MissingJoin) &&
                          !hasRule(*two, RuleId::DeadlockLockOrder),
                      "rules: names select their rules, once each");

        const auto every = ctrace::concurrencyRules(
            {"data-race", "missing-join", "deadlock-lock-order", "condition-wait",
             "fork-after-thread", "unreaped-child", "thread-arg-escape", "unsafe-signal-handler",
             "weak-publication", "thread-arg-freed", "thread-local-escape"},
            error);
        report.expect(every.has_value() && every->enabledRules.size() == 11,
                      "rules: every name of the analyzer's --rules is accepted");

        const auto unknown = ctrace::concurrencyRules({"data-race", "races"}, error);
        report.expect(!unknown.has_value() && error.find("'races'") != std::string::npos &&
                          error.find("data-race") != std::string::npos,
                      "rules: an unknown name is an error that lists the known ones");
    }

    // The finding, field by field. Files are spelled as the input was.
    {
        const std::string input = "tests/concurrency/race.c";
        const std::string absolute = (std::filesystem::current_path() / input).string();
        const ctrace::InputPaths paths({input});
        const ctrace::Diagnostic mapped = ctrace::toDiagnostic(raceFinding(absolute), tool, paths);
        report.expect(mapped.tool == tool && mapped.ruleId == "DataRaceGlobal",
                      "mapping: tool and rule id");
        report.expect(mapped.file == input && mapped.line == 14 && mapped.column == 9,
                      "mapping: the location, spelled as the input");
        report.expect(mapped.severity == ctrace::Severity::Error &&
                          mapped.message == "unsynchronized concurrent access to global 'counter'",
                      "mapping: severity and message are kept as they are");
        report.expect(mapped.cwe == "CWE-362", "mapping: the CWE comes from the CWE taxonomy");
        report.expect(mapped.confidence == "high", "mapping: the confidence is a property");
        report.expect(
            mapped.relatedLocations.size() == 1 && mapped.relatedLocations[0].file == input &&
                mapped.relatedLocations[0].line == 14 && mapped.relatedLocations[0].column == 9 &&
                mapped.relatedLocations[0].message == "Conflicting access",
            "mapping: related locations are kept, spelled as the input");
    }

    // Severities, and the confidence cap: a low-confidence finding is a warning at most.
    {
        const ctrace::InputPaths paths({});
        auto finding = raceFinding("/elsewhere/a.c");
        finding.confidence = ConfidenceLevel::Low;
        report.expect(ctrace::toDiagnostic(finding, tool, paths).severity ==
                          ctrace::Severity::Warning,
                      "mapping: a low-confidence error is a warning");
        finding.severity = ctrace::concurrency::Severity::Info;
        report.expect(ctrace::toDiagnostic(finding, tool, paths).severity == ctrace::Severity::Info,
                      "mapping: the cap never raises a severity");
        finding.severity = ctrace::concurrency::Severity::Warning;
        finding.confidence = std::nullopt;
        const ctrace::Diagnostic plain = ctrace::toDiagnostic(finding, tool, paths);
        report.expect(plain.severity == ctrace::Severity::Warning && plain.confidence.empty(),
                      "mapping: a warning without confidence stays a warning, with no property");
        report.expect(plain.file == "/elsewhere/a.c",
                      "mapping: a file outside the working directory is spelled absolute");

        finding.taxonomies = {{"OWASP", "A1", "Injection"}};
        report.expect(ctrace::toDiagnostic(finding, tool, paths).cwe.empty(),
                      "mapping: no CWE taxonomy, no CWE");
        finding.taxonomies = {{"CWE", "CWE-833", "Deadlock"}};
        report.expect(ctrace::toDiagnostic(finding, tool, paths).cwe == "CWE-833",
                      "mapping: a CWE id already prefixed is not prefixed twice");
        finding.location = {};
        finding.relatedLocations.clear();
        const ctrace::Diagnostic nowhere = ctrace::toDiagnostic(finding, tool, paths);
        report.expect(nowhere.file.empty() && nowhere.line == 0 && nowhere.relatedLocations.empty(),
                      "mapping: no location stays unknown, not resolved against the working "
                      "directory");
    }

    if (report.failures == 0)
    {
        std::cout << "concurrency_analyzer_bridge_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " concurrency analyzer bridge check(s) failed\n";
    return 1;
}
