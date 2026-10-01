// SPDX-License-Identifier: Apache-2.0
#ifndef DIAGNOSTIC_HPP
#define DIAGNOSTIC_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace ctrace
{
    enum class Severity
    {
        Info,
        Warning,
        Error
    };

    /// Another place a finding involves: the other access of a data race, the other lock of a
    /// cycle. Rendered as a note after the finding, and as a SARIF related location.
    struct RelatedLocation
    {
        std::string file; ///< Spelled as the finding's own file is; empty when unknown.
        unsigned line = 0;
        unsigned column = 0;
        std::string message;
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
        // Initialized in place so that the tools' {tool, rule, ..., cwe} initializations may
        // stop at the CWE without -Wmissing-field-initializers.
        std::string confidence{}; ///< "low", "medium" or "high" when the tool rates its finding.
        std::vector<RelatedLocation> relatedLocations{};
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

    /// The order findings are reported in, whatever order the tools finished in: by tool,
    /// file, position and rule, then severity and message so that findings at one position are
    /// ordered too.
    [[nodiscard]] inline bool reportedBefore(const Diagnostic& a, const Diagnostic& b)
    {
        return std::tie(a.tool, a.file, a.line, a.column, a.ruleId, a.severity, a.message) <
               std::tie(b.tool, b.file, b.line, b.column, b.ruleId, b.severity, b.message);
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

    /// `file:line:col`; unknown parts are left out rather than printed as 0.
    [[nodiscard]] inline std::string renderPosition(std::string_view file, unsigned line,
                                                    unsigned column)
    {
        std::string position(file);
        if (line > 0)
        {
            position += ":" + std::to_string(line);
            if (column > 0)
            {
                position += ":" + std::to_string(column);
            }
        }
        return position;
    }

    /// `file:line:col: severity: message [tool/rule]`, the gcc-style line editors and CI
    /// annotators already understand, then one `note:` line per related location.
    [[nodiscard]] inline std::string renderLine(const Diagnostic& diagnostic)
    {
        std::string tag = " [" + diagnostic.tool;
        if (!diagnostic.ruleId.empty())
        {
            tag += "/" + diagnostic.ruleId;
        }
        tag += "]";

        std::string line = renderPosition(diagnostic.file, diagnostic.line, diagnostic.column);
        line += ": ";
        line += severityName(diagnostic.severity);
        line += ": " + diagnostic.message + tag;
        for (const RelatedLocation& related : diagnostic.relatedLocations)
        {
            line += "\n" + renderPosition(related.file, related.line, related.column);
            line += ": note: " + related.message + tag;
        }
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
