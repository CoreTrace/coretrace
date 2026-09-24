// SPDX-License-Identifier: Apache-2.0
#ifndef APP_RUNNER_HPP
#define APP_RUNNER_HPP

#include "Config/config.hpp"
#include "attributes.hpp"

#include <filesystem>
#include <string>

namespace ctrace
{
    /// Logging setup shared by the HTTP server and its tests: requests are handled on a
    /// thread pool, so the logger must be thread-safe and timestamps help correlate lines.
    void configure_server_logging();

    /// Checks the server settings before binding. Returns an empty string when the server may
    /// start, otherwise the reason it must not.
    ///
    /// The API itself is unauthenticated, so a server reachable beyond the loopback interface
    /// must at least carry a shutdown token: it forces the operator to acknowledge exposure.
    CT_NODISCARD std::string validateServerConfig(const ServerConfig& config);

    /// Serves the HTTP API; each request gets what ships next to `executable`, like the CLI.
    CT_NODISCARD int run_server(const ProgramConfig& config,
                                const std::filesystem::path& executable);
    CT_NODISCARD int run_cli_analysis(const ProgramConfig& config);
} // namespace ctrace

#endif // APP_RUNNER_HPP
