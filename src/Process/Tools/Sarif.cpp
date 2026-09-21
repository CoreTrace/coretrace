// SPDX-License-Identifier: Apache-2.0
#include "Process/Tools/Sarif.hpp"

#include <nlohmann/json.hpp>

#include <regex>

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
