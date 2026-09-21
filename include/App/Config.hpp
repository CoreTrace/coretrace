// SPDX-License-Identifier: Apache-2.0
#ifndef APP_CONFIG_HPP
#define APP_CONFIG_HPP

#include "Config/config.hpp"
#include "attributes.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ctrace
{
    /// Outcome of command-line and config-file processing. buildConfig never terminates the
    /// process: help, version and every validation error come back here, and main decides.
    struct ConfigResult
    {
        std::optional<ProgramConfig> config; ///< Present when the program should run.
        std::string output;                  ///< Text for stdout (help, version, notices).
        std::string error;                   ///< Text for stderr.
        std::vector<std::string> warnings;   ///< Deprecation notices, one per line, for stderr.
        int exitCode = 0;                    ///< Process exit code when `config` is absent.
    };

    CT_NODISCARD ConfigResult buildConfig(int argc, char* argv[]);

    /// Where the stack analyzer models shipped with this binary live: the first existing of
    /// `<exe dir>/../config/models` (install prefix, and a `build/` directory inside the repo)
    /// and `<exe dir>/config/models` (any build directory; CMake copies the models there).
    /// Empty when neither exists.
    CT_NODISCARD std::filesystem::path
    defaultModelsDirectory(const std::filesystem::path& executable);

    /// Fills the stack analyzer model paths that are still empty from `modelsDir`. When the
    /// analyzer is selected and a default model is missing, one warning names the model so the
    /// loss of its rule family is visible instead of showing up as a clean report.
    void applyDefaultModels(ProgramConfig& config, const std::filesystem::path& modelsDir,
                            std::vector<std::string>& warnings);
} // namespace ctrace

#endif // APP_CONFIG_HPP
