#include "App/Runner.hpp"

#include "App/Files.hpp"
#include "Process/Ipc/HttpServer.hpp"
#include "Process/Tools/ToolsInvoker.hpp"

#include <coretrace/logger.hpp>

#include <cstdlib>
#include <thread>

namespace ctrace
{
    CT_NODISCARD int run_server(const ProgramConfig& config)
    {
        coretrace::log(coretrace::Level::Info, "Starting in server at {}:{}\n",
                       config.global.serverHost, std::to_string(config.global.serverPort));
        ConsoleLogger logger;
        ApiHandler apiHandler(logger);
        HttpServer server(apiHandler, logger, config.global);
        server.run(config.global.serverHost, config.global.serverPort);
        return EXIT_SUCCESS;
    }

    CT_NODISCARD int run_cli_analysis(const ProgramConfig& config)
    {
        const auto availableThreads = std::thread::hardware_concurrency();
        const auto poolSize = (availableThreads == 0) ? 1U : availableThreads;
        ctrace::ToolInvoker invoker(config, poolSize, config.global.hasAsync);

        if (config.global.hasAsync == std::launch::async)
        {
            coretrace::set_thread_safe(true);
            coretrace::log(coretrace::Level::Info, "Asynchronous execution enabled.\n");
        }

        coretrace::log(coretrace::Level::Debug, "Verbose mode enabled.\n");
        coretrace::log(coretrace::Level::Debug, "Asynchronous execution: {}\n",
                       (config.global.hasAsync == std::launch::async ? "enabled" : "disabled"));
        coretrace::log(coretrace::Level::Debug, "Verbose mode: {}\n",
                       (config.global.verbose ? "enabled" : "disabled"));
        coretrace::log(coretrace::Level::Debug, "Static analysis: {}\n",
                       (config.global.hasStaticAnalysis ? "enabled" : "disabled"));
        coretrace::log(coretrace::Level::Debug, "Dynamic analysis: {}\n",
                       (config.global.hasDynamicAnalysis ? "enabled" : "disabled"));
        coretrace::log(coretrace::Level::Debug, "SARIF format: {}\n",
                       (config.global.hasSarifFormat ? "enabled" : "disabled"));
        coretrace::log(coretrace::Level::Debug, "Report file: {}\n", config.global.report_file);
        coretrace::log(coretrace::Level::Debug, "Entry points: {}\n", config.global.entry_points);
        coretrace::log(coretrace::Level::Debug, "Include compile_commands deps: {}\n",
                       (config.global.include_compdb_deps ? "enabled" : "disabled"));
        if (!config.global.config_file.empty())
        {
            coretrace::log(coretrace::Level::Debug, "Config file in use: {}\n",
                           config.global.config_file);
        }
        else
        {
            coretrace::log(coretrace::Level::Debug,
                           "Config file in use: none (CLI/runtime values only)\n");
        }

        std::vector<std::string> sourceFiles = ctrace::resolveSourceFiles(config);
        if (sourceFiles.empty())
        {
            coretrace::log(coretrace::Level::Error,
                           "No input files resolved. Provide --input or --compile-commands.\n");
            return EXIT_FAILURE;
        }

        for (const auto& file : sourceFiles)
        {
            coretrace::log(coretrace::Level::Info, "Processing file: {}\n", file);
        }

        if (config.global.hasStaticAnalysis)
        {
            coretrace::log(coretrace::Level::Info, "Running static analysis on {} file(s)\n",
                           sourceFiles.size());
            invoker.runStaticTools(sourceFiles);
        }
        if (config.global.hasDynamicAnalysis)
        {
            coretrace::log(coretrace::Level::Info, "Running dynamic analysis on {} file(s)\n",
                           sourceFiles.size());
            invoker.runDynamicTools(sourceFiles);
        }
        if (config.global.hasInvokedSpecificTools)
        {
            coretrace::log(coretrace::Level::Info, "Running specific tools on {} file(s)\n",
                           sourceFiles.size());
            invoker.runSpecificTools(config.global.specificTools, sourceFiles);
        }
        return 0;
    }
} // namespace ctrace
