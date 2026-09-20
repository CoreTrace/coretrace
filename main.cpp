// SPDX-License-Identifier: Apache-2.0
#include "App/Config.hpp"
#include "App/Runner.hpp"

#include <coretrace/logger.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>

namespace
{
    int run(const ctrace::ProgramConfig& config)
    {
        coretrace::enable_logging();
        coretrace::set_prefix("== CoreTrace ==");
        coretrace::set_min_level((config.output.verbose) ? coretrace::Level::Debug
                                                         : coretrace::Level::Info);
        coretrace::set_source_location(false);
        coretrace::set_thread_safe(false);

        if (config.runtime.ipc == "serve")
        {
            return ctrace::run_server(config);
        }
        return ctrace::run_cli_analysis(config);
    }
} // namespace

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

    // Last resort: a failure nobody handled is reported, not a process abort.
    try
    {
        return run(*loaded.config);
    }
    catch (const std::exception& error)
    {
        coretrace::log(coretrace::Level::Error, "{}\n", error.what());
        return EXIT_FAILURE;
    }
    catch (...)
    {
        coretrace::log(coretrace::Level::Error, "Unknown fatal error.\n");
        return EXIT_FAILURE;
    }
}
