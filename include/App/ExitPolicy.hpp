// SPDX-License-Identifier: Apache-2.0
#ifndef APP_EXIT_POLICY_HPP
#define APP_EXIT_POLICY_HPP

#include "Config/config.hpp"
#include "Process/Tools/Diagnostic.hpp"

namespace ctrace
{
    /// Process exit codes. A CI pipeline reads them to tell "findings" from "broken run".
    namespace exit_code
    {
        inline constexpr int kClean = 0;      ///< Analysis complete, nothing at/above --fail-on.
        inline constexpr int kUsage = 1;      ///< Command-line or configuration error.
        inline constexpr int kFindings = 2;   ///< Findings at or above --fail-on.
        inline constexpr int kIncomplete = 3; ///< A tool could not run; results are partial.
    } // namespace exit_code

    struct AnalysisOutcome
    {
        DiagnosticSummary summary;
        bool toolFailed = false; ///< Some tool could not be started, crashed or exited abnormally.
    };

    /// The verdict of a run. An incomplete analysis wins over findings: its counters cannot
    /// be trusted, and `--fail-on none` only silences findings, never a broken toolchain.
    [[nodiscard]] constexpr int exitCodeFor(FailOn policy, const AnalysisOutcome& outcome) noexcept
    {
        if (outcome.toolFailed)
        {
            return exit_code::kIncomplete;
        }
        switch (policy)
        {
        case FailOn::None:
            return exit_code::kClean;
        case FailOn::Warning:
            return (outcome.summary.warning + outcome.summary.error) > 0 ? exit_code::kFindings
                                                                         : exit_code::kClean;
        case FailOn::Error:
            return outcome.summary.error > 0 ? exit_code::kFindings : exit_code::kClean;
        }
        return exit_code::kClean;
    }
} // namespace ctrace

#endif // APP_EXIT_POLICY_HPP
