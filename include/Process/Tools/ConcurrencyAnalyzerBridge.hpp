// SPDX-License-Identifier: Apache-2.0
#ifndef CONCURRENCY_ANALYZER_BRIDGE_HPP
#define CONCURRENCY_ANALYZER_BRIDGE_HPP

#include "Process/Tools/Diagnostic.hpp"
#include "Process/Tools/InputPaths.hpp"

#include <coretrace_concurrency_analysis.hpp>

#include <optional>
#include <string>
#include <vector>

namespace ctrace
{
    /// The rules `names` select, by the names the analyzer's own command line uses (data-race,
    /// missing-join, deadlock-lock-order...); every rule when `names` is empty. Nothing, and
    /// `error` set, for a name the analyzer has no rule for.
    [[nodiscard]] std::optional<concurrency::AnalysisOptions>
    concurrencyRules(const std::vector<std::string>& names, std::string& error);

    /// A finding of the analyzer in CoreTrace's model, attributed to `tool`, its files spelled
    /// through `paths`. Rule id and message are kept as they are; the CWE comes from the CWE
    /// taxonomy; the confidence is a property, and a finding the analyzer rates low is a
    /// warning at most; related locations (the other access of a race) are kept.
    [[nodiscard]] Diagnostic toDiagnostic(const concurrency::Diagnostic& finding,
                                          const std::string& tool, const InputPaths& paths);
} // namespace ctrace

#endif // CONCURRENCY_ANALYZER_BRIDGE_HPP
