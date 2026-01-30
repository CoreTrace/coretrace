#include "App/Config.hpp"
#include "App/Runner.hpp"
#include "ctrace_tools/colors.hpp"

#include <iostream>

int main(int argc, char* argv[])
{
    ctrace::ProgramConfig config = ctrace::buildConfig(argc, argv);

    std::cout << ctrace::Color::GREEN << "CoreTrace - Comprehensive Tracing and Analysis Tool"
              << ctrace::Color::RESET << std::endl;

    if (config.global.ipc == "serve")
    {
        return ctrace::run_server(config);
    }
    return ctrace::run_cli_analysis(config);
}
