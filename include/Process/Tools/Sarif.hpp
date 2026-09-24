// SPDX-License-Identifier: Apache-2.0
#ifndef SARIF_HPP
#define SARIF_HPP

#include "Process/Tools/Diagnostic.hpp"

#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string>
#include <vector>

namespace ctrace
{
    /// Reads every active result of a SARIF 2.1.0 log into the diagnostic model, attributed to
    /// `tool`; suppressed results are not findings. Returns nothing when the document is not a
    /// SARIF log (no `runs` array).
    [[nodiscard]] std::optional<std::vector<Diagnostic>>
    diagnosticsFromSarif(const nlohmann::json& log, const std::string& tool);

    /// Renders every finding of a run as one SARIF 2.1.0 log: one `run` per tool (sorted by
    /// tool name), the rules each tool reported, a `coretrace/v1` partial fingerprint per
    /// result (tool, rule, file and message; not the line, so an alert survives edits above
    /// it) and the CWE as a property. Pure: the caller decides where the document goes.
    [[nodiscard]] nlohmann::json renderSarif(const std::vector<Diagnostic>& diagnostics);

    /// Same, from the raw text a tool printed: the JSON document is located inside the text
    /// (tools print progress lines around it) and parsed. Nothing when no document parses.
    [[nodiscard]] std::optional<std::vector<Diagnostic>>
    diagnosticsFromSarifText(const std::string& text, const std::string& tool);
} // namespace ctrace

#endif // SARIF_HPP
