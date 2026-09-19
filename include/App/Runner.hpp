// SPDX-License-Identifier: Apache-2.0
#ifndef APP_RUNNER_HPP
#define APP_RUNNER_HPP

#include "Config/config.hpp"
#include "attributes.hpp"

namespace ctrace
{
    /// Logging setup shared by the HTTP server and its tests: requests are handled on a
    /// thread pool, so the logger must be thread-safe and timestamps help correlate lines.
    void configure_server_logging();

    CT_NODISCARD int run_server(const ProgramConfig& config);
    CT_NODISCARD int run_cli_analysis(const ProgramConfig& config);
} // namespace ctrace

#endif // APP_RUNNER_HPP
