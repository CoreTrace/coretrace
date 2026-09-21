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
    /// Reads every result of a SARIF 2.1.0 log into the diagnostic model, attributed to
    /// `tool`. Returns nothing when the document is not a SARIF log (no `runs` array).
    [[nodiscard]] std::optional<std::vector<Diagnostic>>
    diagnosticsFromSarif(const nlohmann::json& log, const std::string& tool);

    /// Same, from the raw text a tool printed: the JSON document is located inside the text
    /// (tools print progress lines around it) and parsed. Nothing when no document parses.
    [[nodiscard]] std::optional<std::vector<Diagnostic>>
    diagnosticsFromSarifText(const std::string& text, const std::string& tool);
} // namespace ctrace

#endif // SARIF_HPP
