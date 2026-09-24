// SPDX-License-Identifier: Apache-2.0
#include "Process/Tools/Sarif.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
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
            for (const nlohmann::json& result : *results)
            {
                if (isSuppressed(result))
                {
                    continue;
                }
                Diagnostic diagnostic;
                diagnostic.tool = tool;
                diagnostic.ruleId = result.value("ruleId", "");
                diagnostic.severity = severityFromLevel(result.value("level", "warning"));
                diagnostic.message =
                    result.value("message", nlohmann::json::object()).value("text", "");
                diagnostic.cwe = cweOf(result, diagnostic.message);
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
                nlohmann::json physical;
                physical["artifactLocation"]["uri"] = diagnostic.file;
                if (diagnostic.line > 0)
                {
                    physical["region"]["startLine"] = diagnostic.line;
                    if (diagnostic.column > 0)
                    {
                        physical["region"]["startColumn"] = diagnostic.column;
                    }
                }
                result["locations"] = nlohmann::json::array({{{"physicalLocation", physical}}});
            }
            result["partialFingerprints"]["coretrace/v1"] = fingerprintOf(diagnostic);
            if (!diagnostic.cwe.empty())
            {
                result["properties"]["cwe"] = diagnostic.cwe;
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
                    run["tool"]["driver"]["rules"].push_back({{"id", diagnostic->ruleId}});
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
} // namespace ctrace
