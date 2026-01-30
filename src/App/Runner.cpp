#include "App/Runner.hpp"

#include "App/Files.hpp"
#include "Process/Ipc/HttpServer.hpp"
#include "Process/Tools/ToolsInvoker.hpp"
#include "ctrace_tools/colors.hpp"

#include <cstdlib>
#include <iostream>
#include <thread>

namespace ctrace
{
    CT_NODISCARD int run_server(const ProgramConfig& config)
    {
        std::cout << "\033[36mStarting IPC server at "
                  << config.global.serverHost << ":"
                  << config.global.serverPort << "...\033[0m\n";
        ConsoleLogger logger;
        ApiHandler apiHandler(logger);
        HttpServer server(apiHandler, logger, config.global);
        server.run(config.global.serverHost, config.global.serverPort);
        return EXIT_SUCCESS;
    }

    CT_NODISCARD int run_cli_analysis(const ProgramConfig& config)
    {
        ctrace::ToolInvoker invoker(config, std::thread::hardware_concurrency(), config.global.hasAsync);

        std::cout << ctrace::Color::CYAN << "asynchronous execution: "
                  << (config.global.hasAsync == std::launch::async ? ctrace::Color::GREEN : ctrace::Color::RED)
                  << (config.global.hasAsync == std::launch::async ? "enabled" : "disabled")
                  << ctrace::Color::RESET << std::endl;

        std::cout << ctrace::Color::CYAN << "verbose: "
                  << (config.global.verbose ? ctrace::Color::GREEN : ctrace::Color::RED)
                  << config.global.verbose << ctrace::Color::RESET << std::endl;

        std::cout << ctrace::Color::CYAN << "sarif format: "
                  << (config.global.hasSarifFormat ? ctrace::Color::GREEN : ctrace::Color::RED)
                  << config.global.hasSarifFormat << ctrace::Color::RESET << std::endl;

        std::cout << ctrace::Color::CYAN << "dynamic analysis: "
                  << (config.global.hasDynamicAnalysis ? ctrace::Color::GREEN : ctrace::Color::RED)
                  << config.global.hasDynamicAnalysis << ctrace::Color::RESET << std::endl;

        std::cout << ctrace::Color::CYAN << "Report file: "
                  << ctrace::Color::YELLOW << config.global.report_file
                  << ctrace::Color::RESET << std::endl;

        std::cout << ctrace::Color::CYAN << "entry point: "
                  << ctrace::Color::YELLOW << config.global.entry_points
                  << ctrace::Color::RESET << std::endl;

        std::vector<std::string> sourceFiles = ctrace::resolveSourceFiles(config);

        for (const auto& file : sourceFiles)
        {
            std::cout << ctrace::Color::CYAN << "File: "
                      << ctrace::Color::YELLOW << file << ctrace::Color::RESET << std::endl;

            if (config.global.hasStaticAnalysis)
            {
                std::cout << ctrace::Color::CYAN << "Running static analysis..." << ctrace::Color::RESET << std::endl;
                invoker.runStaticTools(file);
            }
            if (config.global.hasDynamicAnalysis)
            {
                std::cout << ctrace::Color::CYAN << "Running dynamic analysis..." << ctrace::Color::RESET << std::endl;
                invoker.runDynamicTools(file);
            }
            if (config.global.hasInvokedSpecificTools)
            {
                std::cout << ctrace::Color::CYAN << "Running specific tools..." << ctrace::Color::RESET << std::endl;
                invoker.runSpecificTools(config.global.specificTools, file);
            }
        }
        return 0;
    }
}
