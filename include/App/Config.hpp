// SPDX-License-Identifier: Apache-2.0
#ifndef APP_CONFIG_HPP
#define APP_CONFIG_HPP

#include "Config/config.hpp"
#include "attributes.hpp"

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
} // namespace ctrace

#endif // APP_CONFIG_HPP
