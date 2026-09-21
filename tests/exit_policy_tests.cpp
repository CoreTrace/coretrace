// SPDX-License-Identifier: Apache-2.0
//
// The exit code is a verdict on the run: clean, findings at or above --fail-on, or an
// analysis that could not complete. Usage errors keep their own code.
#include "App/ExitPolicy.hpp"

#include <iostream>
#include <string>

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

    ctrace::AnalysisOutcome outcome(std::size_t info, std::size_t warning, std::size_t error,
                                    bool toolFailed = false)
    {
        return {{info, warning, error}, toolFailed};
    }
} // namespace

int main()
{
    using namespace ctrace;
    TestReport report;

    report.expect(parseFailOn("error") == FailOn::Error &&
                      parseFailOn("warning") == FailOn::Warning &&
                      parseFailOn("none") == FailOn::None,
                  "parseFailOn accepts the three levels");
    report.expect(!parseFailOn("info").has_value() && !parseFailOn("").has_value() &&
                      !parseFailOn("Error").has_value(),
                  "parseFailOn rejects anything else, case-sensitively");
    report.expect(failOnName(FailOn::Warning) == "warning", "failOnName round-trips");

    report.expect(exitCodeFor(FailOn::Error, outcome(3, 2, 0)) == exit_code::kClean,
                  "error policy: warnings alone are clean");
    report.expect(exitCodeFor(FailOn::Error, outcome(0, 0, 1)) == exit_code::kFindings,
                  "error policy: one error is a finding");
    report.expect(exitCodeFor(FailOn::Warning, outcome(3, 0, 0)) == exit_code::kClean,
                  "warning policy: info alone is clean");
    report.expect(exitCodeFor(FailOn::Warning, outcome(0, 1, 0)) == exit_code::kFindings,
                  "warning policy: one warning is a finding");
    report.expect(exitCodeFor(FailOn::Warning, outcome(0, 0, 1)) == exit_code::kFindings,
                  "warning policy: an error is at or above the threshold");
    report.expect(exitCodeFor(FailOn::None, outcome(1, 1, 1)) == exit_code::kClean,
                  "none policy: findings never fail the run");
    report.expect(exitCodeFor(FailOn::None, outcome(0, 0, 0, true)) == exit_code::kIncomplete,
                  "none policy: a tool failure still marks the run incomplete");
    report.expect(exitCodeFor(FailOn::Error, outcome(0, 0, 5, true)) == exit_code::kIncomplete,
                  "a tool failure takes precedence over findings");
    report.expect(exit_code::kClean == 0 && exit_code::kUsage == 1 && exit_code::kFindings == 2 &&
                      exit_code::kIncomplete == 3,
                  "the four codes are distinct and documented");

    if (report.failures == 0)
    {
        std::cout << "exit_policy_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " exit policy check(s) failed\n";
    return 1;
}
