// SPDX-License-Identifier: Apache-2.0
#include "Process/Tools/ConcurrencyAnalyzerBridge.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace ctrace
{
    namespace
    {
        using concurrency::RuleId;

        /// The analyzer's rules by the names its `--rules=` option takes.
        constexpr std::array<std::pair<std::string_view, RuleId>, 11> kRules = {{
            {"data-race", RuleId::DataRaceGlobal},
            {"missing-join", RuleId::MissingJoin},
            {"deadlock-lock-order", RuleId::DeadlockLockOrder},
            {"condition-wait", RuleId::ConditionWaitWithoutPredicate},
            {"fork-after-thread", RuleId::ForkAfterThreadCreation},
            {"unreaped-child", RuleId::UnreapedChildProcess},
            {"thread-arg-escape", RuleId::ThreadArgumentEscapesFrame},
            {"unsafe-signal-handler", RuleId::UnsafeSignalHandler},
            {"weak-publication", RuleId::WeakPublicationOrdering},
            {"thread-arg-freed", RuleId::ThreadArgumentFreedEarly},
            {"thread-local-escape", RuleId::ThreadLocalOutlivesThread},
        }};

        [[nodiscard]] std::string knownRuleNames()
        {
            std::string names;
            for (const auto& rule : kRules)
            {
                if (!names.empty())
                {
                    names += ", ";
                }
                names += rule.first;
            }
            return names;
        }

        [[nodiscard]] Severity toSeverity(concurrency::Severity severity)
        {
            switch (severity)
            {
            case concurrency::Severity::Info:
                return Severity::Info;
            case concurrency::Severity::Error:
                return Severity::Error;
            case concurrency::Severity::Warning:
                break;
            }
            return Severity::Warning;
        }

        /// An unknown location stays unknown rather than resolving to the working directory.
        [[nodiscard]] std::string spelled(const InputPaths& paths, const std::string& file)
        {
            return file.empty() ? file : paths.display(file);
        }

        [[nodiscard]] std::string cweOf(const concurrency::Diagnostic& finding)
        {
            for (const concurrency::TaxonomyRef& taxonomy : finding.taxonomies)
            {
                if (taxonomy.scheme == "CWE")
                {
                    return taxonomy.id.starts_with("CWE-") ? taxonomy.id : "CWE-" + taxonomy.id;
                }
            }
            return {};
        }
    } // namespace

    std::optional<concurrency::AnalysisOptions>
    concurrencyRules(const std::vector<std::string>& names, std::string& error)
    {
        error.clear();
        if (names.empty())
        {
            return concurrency::AnalysisOptions::allAvailable();
        }
        concurrency::AnalysisOptions options;
        options.enabledRules.clear();
        for (const std::string& name : names)
        {
            const auto rule = std::find_if(kRules.begin(), kRules.end(),
                                           [&](const auto& entry) { return entry.first == name; });
            if (rule == kRules.end())
            {
                error = "Unknown rule '" + name +
                        "' in tools.coretrace-concurrency-analyzer.rules. Known rules: " +
                        knownRuleNames();
                return std::nullopt;
            }
            if (!options.isEnabled(rule->second))
            {
                options.enabledRules.push_back(rule->second);
            }
        }
        return options;
    }

    Diagnostic toDiagnostic(const concurrency::Diagnostic& finding, const std::string& tool,
                            const InputPaths& paths)
    {
        Diagnostic diagnostic;
        diagnostic.tool = tool;
        diagnostic.ruleId = std::string(concurrency::toString(finding.ruleId));
        diagnostic.file = spelled(paths, finding.location.file);
        diagnostic.line = finding.location.line;
        diagnostic.column = finding.location.column;
        diagnostic.severity = toSeverity(finding.severity);
        diagnostic.message = finding.message;
        diagnostic.cwe = cweOf(finding);
        if (finding.confidence.has_value())
        {
            diagnostic.confidence = std::string(concurrency::toString(*finding.confidence));
            // The analyzer says so itself: a may-alias race is a lead to look at, not a verdict
            // to fail a build over.
            if (*finding.confidence == concurrency::ConfidenceLevel::Low &&
                diagnostic.severity == Severity::Error)
            {
                diagnostic.severity = Severity::Warning;
            }
        }
        for (const concurrency::RelatedLocation& related : finding.relatedLocations)
        {
            diagnostic.relatedLocations.push_back({spelled(paths, related.location.file),
                                                   related.location.line, related.location.column,
                                                   related.label});
        }
        return diagnostic;
    }
} // namespace ctrace
