// SPDX-License-Identifier: Apache-2.0
#ifndef DIAGNOSTIC_HPP
#define DIAGNOSTIC_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ctrace
{
    enum class Severity
    {
        Info,
        Warning,
        Error
    };

    /// One finding, whatever tool produced it. Every tool normalizes its own format to this
    /// model; counting, gating and merging only ever see this.
    struct Diagnostic
    {
        std::string tool;   ///< Registered tool name, e.g. "cppcheck".
        std::string ruleId; ///< Tool-specific rule identifier; empty when the tool has none.
        std::string file;   ///< Path as reported by the tool; empty when unknown.
        unsigned line = 0;  ///< 1-based; 0 when unknown.
        unsigned column = 0;
        Severity severity = Severity::Warning;
        std::string message;
        std::string cwe; ///< "CWE-415" style identifier when the tool reports one.
    };

    struct DiagnosticSummary
    {
        std::size_t info = 0;
        std::size_t warning = 0;
        std::size_t error = 0;
    };

    [[nodiscard]] constexpr std::string_view severityName(Severity severity) noexcept
    {
        switch (severity)
        {
        case Severity::Info:
            return "info";
        case Severity::Warning:
            return "warning";
        case Severity::Error:
            return "error";
        }
        return "warning";
    }

    [[nodiscard]] inline DiagnosticSummary summarize(const std::vector<Diagnostic>& diagnostics)
    {
        DiagnosticSummary summary;
        for (const Diagnostic& diagnostic : diagnostics)
        {
            switch (diagnostic.severity)
            {
            case Severity::Info:
                ++summary.info;
                break;
            case Severity::Warning:
                ++summary.warning;
                break;
            case Severity::Error:
                ++summary.error;
                break;
            }
        }
        return summary;
    }

    /// `file:line:col: severity: message [tool/rule]`, the gcc-style line editors and CI
    /// annotators already understand. Unknown parts are left out rather than printed as 0.
    [[nodiscard]] inline std::string renderLine(const Diagnostic& diagnostic)
    {
        std::string line = diagnostic.file;
        if (diagnostic.line > 0)
        {
            line += ":" + std::to_string(diagnostic.line);
            if (diagnostic.column > 0)
            {
                line += ":" + std::to_string(diagnostic.column);
            }
        }
        line += ": ";
        line += severityName(diagnostic.severity);
        line += ": " + diagnostic.message + " [" + diagnostic.tool;
        if (!diagnostic.ruleId.empty())
        {
            line += "/" + diagnostic.ruleId;
        }
        line += "]";
        return line;
    }

    [[nodiscard]] inline std::string renderLines(const std::vector<Diagnostic>& diagnostics)
    {
        std::string text;
        for (const Diagnostic& diagnostic : diagnostics)
        {
            if (!text.empty())
            {
                text += '\n';
            }
            text += renderLine(diagnostic);
        }
        return text;
    }
} // namespace ctrace

#endif // DIAGNOSTIC_HPP
