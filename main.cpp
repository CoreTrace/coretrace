#include "App/Config.hpp"
#include "App/Runner.hpp"

#include <coretrace/logger.hpp>

int main(int argc, char* argv[])
{
    ctrace::ProgramConfig config = ctrace::buildConfig(argc, argv);

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
        coretrace::set_timestamps(true);
        return ctrace::run_server(config);
    }
    return ctrace::run_cli_analysis(config);
}
