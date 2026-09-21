// SPDX-License-Identifier: Apache-2.0
#include "Process/Tools/Sarif.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <map>
#include <regex>
#include <set>
#include <string_view>

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

    nlohmann::json renderSarif(const std::vector<Diagnostic>& diagnostics)
    {
        std::map<std::string, std::vector<const Diagnostic*>> byTool;
        for (const Diagnostic& diagnostic : diagnostics)
        {
            byTool[diagnostic.tool].push_back(&diagnostic);
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
                run["results"].push_back(resultOf(*diagnostic));
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
