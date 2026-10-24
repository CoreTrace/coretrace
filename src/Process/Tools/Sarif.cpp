// SPDX-License-Identifier: Apache-2.0
#include "Process/Tools/Sarif.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <map>
#include <regex>
#include <set>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>

namespace ctrace
{
    namespace
    {
        [[nodiscard]] Severity severityFromLevel(const std::string& level)
        {
            if (level == "error")
            {
                return Severity::Error;
            }
            if (level == "note" || level == "none")
            {
                return Severity::Info;
            }
            return Severity::Warning;
        }

        /// The CWE of a result: an explicit property when the tool sets one, else the first
        /// "CWE-<n>" mentioned in the message (flawfinder's convention).
        [[nodiscard]] std::string cweOf(const nlohmann::json& result, const std::string& message)
        {
            if (const auto properties = result.find("properties");
                properties != result.end() && properties->is_object())
            {
                if (const auto cwe = properties->find("cwe");
                    cwe != properties->end() && cwe->is_string())
                {
                    return cwe->get<std::string>();
                }
            }
            static const std::regex cwePattern(R"(CWE-\d+)");
            std::smatch match;
            if (std::regex_search(message, match, cwePattern))
            {
                return match.str();
            }
            return {};
        }

        /// SARIF 2.1.0 §3.27.23: a result is suppressed when one of its suppressions is
        /// accepted, explicitly or by omitting the status; under review or rejected ones
        /// leave it active.
        [[nodiscard]] bool isSuppressed(const nlohmann::json& result)
        {
            const auto suppressions = result.find("suppressions");
            if (suppressions == result.end() || !suppressions->is_array())
            {
                return false;
            }
            for (const nlohmann::json& suppression : *suppressions)
            {
                if (suppression.is_object() &&
                    suppression.value("status", "accepted") == "accepted")
                {
                    return true;
                }
            }
            return false;
        }

        /// The text of the SARIF message `key` of `rule`; empty when it has none, or only repeats
        /// the rule's `id`, which says nothing about it.
        [[nodiscard]] std::string textOf(const nlohmann::json& rule, const char* key,
                                         const std::string& id)
        {
            const auto message = rule.find(key);
            if (message == rule.end() || !message->is_object())
            {
                return {};
            }
            const auto text = message->find("text");
            if (text == message->end() || !text->is_string() || *text == id)
            {
                return {};
            }
            return text->get<std::string>();
        }

        /// What each rule of a run's tool checks, by rule id, as `tool.driver.rules` describes
        /// it. The texts are optional: a list that is not well formed describes nothing rather
        /// than failing the tool.
        [[nodiscard]] std::map<std::string, RuleDescription>
        ruleDescriptionsOf(const nlohmann::json& run)
        {
            std::map<std::string, RuleDescription> described;
            const nlohmann::json* rules = &run;
            for (const char* key : {"tool", "driver", "rules"})
            {
                const auto member = rules->find(key);
                if (member == rules->end())
                {
                    return described;
                }
                rules = &*member;
            }
            if (!rules->is_array())
            {
                return described;
            }
            for (const nlohmann::json& rule : *rules)
            {
                const auto id = rule.find("id");
                if (id == rule.end() || !id->is_string())
                {
                    continue;
                }
                const std::string ruleId = id->get<std::string>();
                const auto helpUri = rule.find("helpUri");
                described[ruleId] = {textOf(rule, "shortDescription", ruleId),
                                     textOf(rule, "fullDescription", ruleId),
                                     helpUri != rule.end() && helpUri->is_string()
                                         ? helpUri->get<std::string>()
                                         : std::string()};
            }
            return described;
        }

        /// The text of a SARIF message without its embedded links (SARIF 2.1.0 §3.11.6): each
        /// `[text](id)` becomes its text, and the brackets and backslashes escaped around them
        /// become themselves. The merged log links the message to its related locations again.
        [[nodiscard]] std::string plainText(std::string_view text)
        {
            std::string plain;
            for (std::size_t at = 0; at < text.size(); ++at)
            {
                const char c = text[at];
                if (c == '\\' && at + 1 < text.size() &&
                    (text[at + 1] == '\\' || text[at + 1] == '[' || text[at + 1] == ']'))
                {
                    plain += text[++at];
                    continue;
                }
                if (c == '[')
                {
                    // An embedded link: an unescaped ']' right before '(' and a closing ')'.
                    std::size_t close = at + 1;
                    while (close < text.size() && text[close] != ']')
                        close += text[close] == '\\' ? 2 : 1;
                    const std::size_t end = close + 1 < text.size() && text[close + 1] == '('
                                                ? text.find(')', close + 2)
                                                : std::string_view::npos;
                    if (close < text.size() && end != std::string_view::npos)
                    {
                        plain += plainText(text.substr(at + 1, close - at - 1));
                        at = end;
                        continue;
                    }
                }
                plain += c;
            }
            return plain;
        }

        /// The confidence a result's properties give, `low`, `medium` or `high` in any case;
        /// empty for another value or none.
        [[nodiscard]] std::string confidenceOf(const nlohmann::json& result)
        {
            const auto properties = result.find("properties");
            if (properties == result.end() || !properties->is_object())
                return {};
            const auto confidence = properties->find("confidence");
            if (confidence == properties->end() || !confidence->is_string())
                return {};
            std::string level = confidence->get<std::string>();
            std::transform(level.begin(), level.end(), level.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return level == "low" || level == "medium" || level == "high" ? level : std::string();
        }

        /// The related locations of a result: each place, when it has one, and its message.
        /// An entry that is not an object is skipped.
        [[nodiscard]] std::vector<RelatedLocation> relatedLocationsOf(const nlohmann::json& result)
        {
            std::vector<RelatedLocation> related;
            const auto locations = result.find("relatedLocations");
            if (locations == result.end() || !locations->is_array())
                return related;
            for (const nlohmann::json& location : *locations)
            {
                if (!location.is_object())
                    continue;
                RelatedLocation place;
                const nlohmann::json& physical =
                    location.value("physicalLocation", nlohmann::json::object());
                if (physical.is_object())
                {
                    const nlohmann::json& artifact =
                        physical.value("artifactLocation", nlohmann::json::object());
                    const nlohmann::json& region =
                        physical.value("region", nlohmann::json::object());
                    if (artifact.is_object())
                        place.file = artifact.value("uri", "");
                    if (region.is_object())
                    {
                        place.line = region.value("startLine", 0U);
                        place.column = region.value("startColumn", 0U);
                    }
                }
                const nlohmann::json& message = location.value("message", nlohmann::json::object());
                if (message.is_object())
                    place.message = plainText(message.value("text", ""));
                related.push_back(std::move(place));
            }
            return related;
        }

        void readLocation(const nlohmann::json& result, Diagnostic& diagnostic)
        {
            const auto locations = result.find("locations");
            if (locations == result.end() || !locations->is_array() || locations->empty())
            {
                return;
            }
            const nlohmann::json& physical =
                locations->front().value("physicalLocation", nlohmann::json::object());
            diagnostic.file =
                physical.value("artifactLocation", nlohmann::json::object()).value("uri", "");
            const nlohmann::json& region = physical.value("region", nlohmann::json::object());
            diagnostic.line = region.value("startLine", 0U);
            diagnostic.column = region.value("startColumn", 0U);
        }
    } // namespace

    std::optional<std::vector<Diagnostic>> diagnosticsFromSarif(const nlohmann::json& log,
                                                                const std::string& tool)
    {
        if (!log.is_object() || !log.contains("runs") || !log["runs"].is_array())
        {
            return std::nullopt;
        }
        std::vector<Diagnostic> diagnostics;
        for (const nlohmann::json& run : log["runs"])
        {
            const auto results = run.find("results");
            if (results == run.end() || !results->is_array())
            {
                continue;
            }
            const std::map<std::string, RuleDescription> described = ruleDescriptionsOf(run);
            for (const nlohmann::json& result : *results)
            {
                if (isSuppressed(result))
                {
                    continue;
                }
                Diagnostic diagnostic;
                diagnostic.tool = tool;
                diagnostic.ruleId = result.value("ruleId", "");
                if (const auto rule = described.find(diagnostic.ruleId); rule != described.end())
                {
                    diagnostic.ruleDescription = rule->second;
                }
                diagnostic.severity = severityFromLevel(result.value("level", "warning"));
                diagnostic.message =
                    plainText(result.value("message", nlohmann::json::object()).value("text", ""));
                diagnostic.cwe = cweOf(result, diagnostic.message);
                diagnostic.confidence = confidenceOf(result);
                diagnostic.relatedLocations = relatedLocationsOf(result);
                readLocation(result, diagnostic);
                diagnostics.push_back(std::move(diagnostic));
            }
        }
        return diagnostics;
    }

    namespace
    {
        constexpr std::string_view kSarifSchema =
            "https://docs.oasis-open.org/sarif/sarif/v2.1.0/errata01/os/schemas/"
            "sarif-schema-2.1.0.json";

        [[nodiscard]] constexpr std::string_view levelOf(Severity severity) noexcept
        {
            switch (severity)
            {
            case Severity::Info:
                return "note";
            case Severity::Warning:
                return "warning";
            case Severity::Error:
                return "error";
            }
            return "warning";
        }

        /// FNV-1a over the identity fields, in hex. Deterministic across platforms, unlike
        /// std::hash, which is what "the same alert next run" needs.
        [[nodiscard]] std::string fingerprintOf(const Diagnostic& diagnostic)
        {
            std::uint64_t hash = 14695981039346656037ULL;
            const auto feed = [&hash](std::string_view text)
            {
                for (const unsigned char ch : text)
                {
                    hash ^= ch;
                    hash *= 1099511628211ULL;
                }
                hash ^= 0xFFU;
                hash *= 1099511628211ULL;
            };
            feed(diagnostic.tool);
            feed(diagnostic.ruleId);
            feed(diagnostic.file);
            feed(diagnostic.message);
            constexpr std::string_view digits = "0123456789abcdef";
            std::string hex(16, '0');
            for (std::size_t i = 0; i < 16; ++i)
            {
                hex[15 - i] = digits[hash & 0xFU];
                hash >>= 4;
            }
            return hex;
        }

        [[nodiscard]] nlohmann::json physicalLocationOf(const std::string& file, unsigned line,
                                                        unsigned column)
        {
            nlohmann::json physical;
            physical["artifactLocation"]["uri"] = file;
            if (line > 0)
            {
                physical["region"]["startLine"] = line;
                if (column > 0)
                {
                    physical["region"]["startColumn"] = column;
                }
            }
            return physical;
        }

        [[nodiscard]] nlohmann::json resultOf(const Diagnostic& diagnostic)
        {
            nlohmann::json result;
            if (!diagnostic.ruleId.empty())
            {
                result["ruleId"] = diagnostic.ruleId;
            }
            result["level"] = levelOf(diagnostic.severity);
            result["message"]["text"] = diagnostic.message;
            if (!diagnostic.file.empty())
            {
                result["locations"] = nlohmann::json::array(
                    {{{"physicalLocation",
                       physicalLocationOf(diagnostic.file, diagnostic.line, diagnostic.column)}}});
            }
            for (const RelatedLocation& related : diagnostic.relatedLocations)
            {
                nlohmann::json location;
                if (!related.file.empty())
                {
                    location["physicalLocation"] =
                        physicalLocationOf(related.file, related.line, related.column);
                }
                location["message"]["text"] = related.message;
                result["relatedLocations"].push_back(std::move(location));
            }
            result["partialFingerprints"]["coretrace/v1"] = fingerprintOf(diagnostic);
            if (!diagnostic.cwe.empty())
            {
                result["properties"]["cwe"] = diagnostic.cwe;
            }
            if (!diagnostic.confidence.empty())
            {
                result["properties"]["confidence"] = diagnostic.confidence;
            }
            return result;
        }
    } // namespace

    namespace
    {
        [[nodiscard]] constexpr int rankOf(Severity severity) noexcept
        {
            return static_cast<int>(severity);
        }

        /// A rule as the log lists it: its id, and what its tool says it checks, the parts the
        /// tool gave only.
        [[nodiscard]] nlohmann::json ruleOf(const Diagnostic& diagnostic)
        {
            nlohmann::json rule = {{"id", diagnostic.ruleId}};
            const RuleDescription& described = diagnostic.ruleDescription;
            if (!described.title.empty())
            {
                rule["shortDescription"]["text"] = described.title;
            }
            if (!described.description.empty())
            {
                rule["fullDescription"]["text"] = described.description;
            }
            if (!described.helpUri.empty())
            {
                rule["helpUri"] = described.helpUri;
            }
            return rule;
        }

        /// The reports of other tools that a kept result stands for.
        using Duplicates = std::map<const Diagnostic*, std::vector<const Diagnostic*>>;

        /// Groups the reports of one weakness (same file, however spelled, same line, same CWE)
        /// made by different tools. In each group the most severe report is kept (the first
        /// tool by name on a tie); the others are listed against it and left out. Reports
        /// without a file, a line or a CWE cannot be matched and are all kept.
        [[nodiscard]] Duplicates findDuplicates(const std::vector<Diagnostic>& diagnostics,
                                                std::set<const Diagnostic*>& merged)
        {
            std::map<std::tuple<std::string, unsigned, std::string>, std::vector<const Diagnostic*>>
                byFlaw;
            for (const Diagnostic& diagnostic : diagnostics)
            {
                if (diagnostic.file.empty() || diagnostic.line == 0 || diagnostic.cwe.empty())
                {
                    continue;
                }
                std::error_code err;
                const std::filesystem::path absolute =
                    std::filesystem::absolute(diagnostic.file, err);
                const std::string file = (err ? std::filesystem::path(diagnostic.file) : absolute)
                                             .lexically_normal()
                                             .string();
                byFlaw[{file, diagnostic.line, diagnostic.cwe}].push_back(&diagnostic);
            }

            Duplicates duplicates;
            for (const auto& [flaw, reports] : byFlaw)
            {
                const Diagnostic* kept =
                    *std::min_element(reports.begin(), reports.end(),
                                      [](const Diagnostic* a, const Diagnostic* b)
                                      {
                                          return std::make_pair(-rankOf(a->severity), a->tool) <
                                                 std::make_pair(-rankOf(b->severity), b->tool);
                                      });
                for (const Diagnostic* report : reports)
                {
                    if (report->tool != kept->tool)
                    {
                        duplicates[kept].push_back(report);
                        merged.insert(report);
                    }
                }
            }
            return duplicates;
        }
    } // namespace

    nlohmann::json renderSarif(const std::vector<Diagnostic>& diagnostics)
    {
        std::set<const Diagnostic*> merged;
        const Duplicates duplicates = findDuplicates(diagnostics, merged);

        std::map<std::string, std::vector<const Diagnostic*>> byTool;
        for (const Diagnostic& diagnostic : diagnostics)
        {
            // A tool whose every report was merged into another's still ran: it keeps a run.
            auto& reports = byTool[diagnostic.tool];
            if (merged.count(&diagnostic) == 0)
            {
                reports.push_back(&diagnostic);
            }
        }

        nlohmann::json log;
        log["version"] = "2.1.0";
        log["$schema"] = kSarifSchema;
        log["runs"] = nlohmann::json::array();
        for (const auto& [tool, items] : byTool)
        {
            nlohmann::json run;
            run["tool"]["driver"]["name"] = tool;
            run["tool"]["driver"]["rules"] = nlohmann::json::array();
            run["results"] = nlohmann::json::array();
            std::set<std::string> rules;
            for (const Diagnostic* diagnostic : items)
            {
                if (!diagnostic->ruleId.empty() && rules.insert(diagnostic->ruleId).second)
                {
                    run["tool"]["driver"]["rules"].push_back(ruleOf(*diagnostic));
                }
                nlohmann::json result = resultOf(*diagnostic);
                if (const auto others = duplicates.find(diagnostic); others != duplicates.end())
                {
                    for (const Diagnostic* other : others->second)
                    {
                        result["properties"]["alsoReportedBy"].push_back(
                            {{"tool", other->tool}, {"ruleId", other->ruleId}});
                    }
                }
                run["results"].push_back(std::move(result));
            }
            log["runs"].push_back(std::move(run));
        }
        return log;
    }

    std::optional<std::vector<Diagnostic>> diagnosticsFromSarifText(const std::string& text,
                                                                    const std::string& tool)
    {
        const auto begin = text.find('{');
        const auto end = text.rfind('}');
        if (begin == std::string::npos || end == std::string::npos || end < begin)
        {
            return std::nullopt;
        }
        const nlohmann::json log = nlohmann::json::parse(
            text.begin() + static_cast<std::ptrdiff_t>(begin),
            text.begin() + static_cast<std::ptrdiff_t>(end) + 1, nullptr, false);
        if (log.is_discarded())
        {
            return std::nullopt;
        }
        return diagnosticsFromSarif(log, tool);
    }

    std::string sarifText(const nlohmann::json& log)
    {
        return log.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
    }
} // namespace ctrace
