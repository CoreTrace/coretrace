// SPDX-License-Identifier: Apache-2.0
#include "App/Config.hpp"
#include "App/Runner.hpp"

#include <coretrace/logger.hpp>

#include <iostream>

int main(int argc, char* argv[])
{
    const ctrace::ConfigResult loaded = ctrace::buildConfig(argc, argv);
    if (!loaded.output.empty())
    {
        std::cout << loaded.output;
    }
    if (!loaded.error.empty())
    {
        std::cerr << loaded.error;
    }
    if (!loaded.config.has_value())
    {
        return loaded.exitCode;
    }
    const ctrace::ProgramConfig& config = *loaded.config;

    // std::cout << ctrace::Color::GREEN << "CoreTrace - Comprehensive Tracing and Analysis Tool"
    //           << ctrace::Color::RESET << std::endl;

    coretrace::enable_logging();
    coretrace::set_prefix("== CoreTrace ==");
    coretrace::set_min_level((config.global.verbose) ? coretrace::Level::Debug
                                                     : coretrace::Level::Info);
    coretrace::set_source_location(false);
    coretrace::set_thread_safe(false);

    if (config.global.ipc == "serve")
    {
        return ctrace::run_server(config);
    }
    return ctrace::run_cli_analysis(config);
}
