#ifndef APP_RUNNER_HPP
#define APP_RUNNER_HPP

#include "Config/config.hpp"
#include "attributes.hpp"

namespace ctrace
{
    CT_NODISCARD int run_server(const ProgramConfig& config);
    CT_NODISCARD int run_cli_analysis(const ProgramConfig& config);
}

#endif // APP_RUNNER_HPP
