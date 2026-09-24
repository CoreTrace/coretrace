// SPDX-License-Identifier: Apache-2.0
#ifndef APP_SHIPPED_DEFAULTS_HPP
#define APP_SHIPPED_DEFAULTS_HPP

#include "Config/config.hpp"
#include "attributes.hpp"

#include <filesystem>
#include <string>
#include <vector>

/// What ships next to the ctrace binary and fills the settings a user left empty: the stack
/// analyzer models and the bundled external tools. The CLI and the HTTP server apply it the
/// same way, after the config file and the request, so an explicit value always wins.
namespace ctrace
{
    /// Applies every shipped default for the binary at `executable`. An empty `executable`
    /// (location unknown) ships nothing; missing models are then reported in `warnings`.
    void applyShippedDefaults(ProgramConfig& config, const std::filesystem::path& executable,
                              std::vector<std::string>& warnings);

    /// Where the stack analyzer models shipped with this binary live: the first existing of
    /// `<exe dir>/../config/models` (install prefix, and a `build/` directory inside the repo)
    /// and `<exe dir>/config/models` (any build directory; CMake copies the models there).
    /// Empty when neither exists.
    CT_NODISCARD std::filesystem::path
    defaultModelsDirectory(const std::filesystem::path& executable);

    /// Points every external tool without a configured path at the copy shipped with the binary
    /// at `executable`, when there is one: the first executable of
    /// `<exe dir>/../libexec/coretrace/<tool>/<tool>` (install prefix) and
    /// `<exe dir>/libexec/coretrace/<tool>/<tool>` (build tree). Other tools keep being resolved
    /// through PATH, so a release archive runs without anything installed.
    void applyBundledTools(ProgramConfig& config, const std::filesystem::path& executable);

    /// Fills the stack analyzer model paths that are still empty from `modelsDir`. When the
    /// analyzer is selected and a default model is missing, one warning names the model so the
    /// loss of its rule family is visible instead of showing up as a clean report.
    void applyDefaultModels(ProgramConfig& config, const std::filesystem::path& modelsDir,
                            std::vector<std::string>& warnings);
} // namespace ctrace

#endif // APP_SHIPPED_DEFAULTS_HPP
