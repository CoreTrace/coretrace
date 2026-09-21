// SPDX-License-Identifier: Apache-2.0
#ifndef APP_TOOL_CONFIG_HPP
#define APP_TOOL_CONFIG_HPP

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "Config/config.hpp"

namespace ctrace
{
    /// Loads a JSON config file into `config` (defaults < file). Relative paths in the file
    /// resolve from its directory; `config.config_file` records the file path.
    /// `warnings`, when given, receives one line per deprecated spelling found in the
    /// document, naming the canonical key to use instead.
    bool applyToolConfigFile(ProgramConfig& config, std::string_view configPath,
                             std::string& errorMessage,
                             std::vector<std::string>* warnings = nullptr);

    /// Applies an already-parsed config document (the same sectioned schema as the file) to
    /// `config`. `configDir` is the base for relative paths; empty leaves them as given.
    /// This is the single validation and assignment path for every configuration input.
    bool applyToolConfigObject(ProgramConfig& config, const nlohmann::json& root,
                               const std::filesystem::path& configDir, std::string& errorMessage,
                               std::vector<std::string>* warnings = nullptr);
} // namespace ctrace

#endif // APP_TOOL_CONFIG_HPP
