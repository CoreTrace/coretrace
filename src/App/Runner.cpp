// SPDX-License-Identifier: Apache-2.0
#include "App/Runner.hpp"

#include "App/Files.hpp"
#include "Process/Ipc/HttpServer.hpp"
#include "Process/Tools/ToolsInvoker.hpp"

#include <coretrace/logger.hpp>

#include <cstdlib>
#include <thread>
#include <unordered_set>

namespace ctrace
{
    void configure_server_logging()
    {
        coretrace::set_timestamps(true);
        coretrace::set_thread_safe(true);
    }

    CT_NODISCARD std::string validateServerConfig(const ServerConfig& config)
    {
        static const std::unordered_set<std::string> loopbackHosts = {
            "",
            "127.0.0.1",
            "::1",
            "localhost",
        };
        if (loopbackHosts.count(config.host) != 0 || config.host.rfind("127.", 0) == 0)
        {
            return {};
        }
        if (config.shutdown_token.empty())
        {
            return "Refusing to serve on '" + config.host +
                   "': the API is unauthenticated, so a host reachable beyond the loopback "
                   "interface requires server.shutdown_token (--shutdown-token).";
        }
        return {};
    }

    CT_NODISCARD int run_server(const ProgramConfig& config)
    {
        configure_server_logging();
        if (const std::string error = validateServerConfig(config.server); !error.empty())
        {
            coretrace::log(coretrace::Level::Error, "{}\n", error);
            return EXIT_FAILURE;
        }
        coretrace::log(coretrace::Level::Info, "Starting in server at {}:{}\n", config.server.host,
                       std::to_string(config.server.port));
        ConsoleLogger logger;
        ApiHandler apiHandler(logger);
        HttpServer server(apiHandler, logger, config.server);
        server.run(config.server.host, config.server.port);
        return EXIT_SUCCESS;
    }

    CT_NODISCARD int run_cli_analysis(const ProgramConfig& config)
    {
        const auto availableThreads = std::thread::hardware_concurrency();
        const auto poolSize = (availableThreads == 0) ? 1U : availableThreads;
        ctrace::ToolInvoker invoker(
            config, poolSize, (config.runtime.async ? std::launch::async : std::launch::deferred));

        if (config.runtime.async)
        {
            coretrace::set_thread_safe(true);
            coretrace::log(coretrace::Level::Debug, "Asynchronous execution enabled.\n");
        }

        coretrace::log(coretrace::Level::Debug, "Verbose mode enabled.\n");
        coretrace::log(coretrace::Level::Debug, "Asynchronous execution: {}\n",
                       (config.runtime.async ? "enabled" : "disabled"));
        coretrace::log(coretrace::Level::Debug, "Verbose mode: {}\n",
                       (config.output.verbose ? "enabled" : "disabled"));
        coretrace::log(coretrace::Level::Debug, "Static analysis: {}\n",
                       (config.analysis.static_enabled ? "enabled" : "disabled"));
        coretrace::log(coretrace::Level::Debug, "Dynamic analysis: {}\n",
                       (config.analysis.dynamic_enabled ? "enabled" : "disabled"));
        coretrace::log(coretrace::Level::Debug, "SARIF format: {}\n",
                       (config.output.sarif_format ? "enabled" : "disabled"));
        coretrace::log(coretrace::Level::Debug, "Report file: {}\n", config.output.report_file);
        coretrace::log(coretrace::Level::Debug, "Entry points: {}\n",
                       ctrace_tools::strings::joinByComma(config.files.entry_points));
        coretrace::log(coretrace::Level::Debug, "Include compile_commands deps: {}\n",
                       (config.files.include_compdb_deps ? "enabled" : "disabled"));
        if (!config.config_file.empty())
        {
            coretrace::log(coretrace::Level::Debug, "Config file in use: {}\n", config.config_file);
        }
        else
        {
            coretrace::log(coretrace::Level::Debug,
                           "Config file in use: none (CLI/runtime values only)\n");
        }

        ctrace::SourceFileResolution resolution = ctrace::resolveSourceFiles(config);
        if (!resolution.ok())
        {
            coretrace::log(coretrace::Level::Error, "{}\n", resolution.error);
            return EXIT_FAILURE;
        }
        const std::vector<std::string>& sourceFiles = resolution.files;
        if (sourceFiles.empty())
        {
            coretrace::log(coretrace::Level::Error,
                           "No input files resolved. Provide --input or --compile-commands.\n");
            return EXIT_FAILURE;
        }

        for (const auto& file : sourceFiles)
        {
            coretrace::log(coretrace::Level::Debug, "Processing file: {}\n", file);
        }

        if (config.analysis.static_enabled)
        {
            coretrace::log(coretrace::Level::Info, "Running static analysis on {} file(s)\n",
                           sourceFiles.size());
            invoker.runStaticTools(sourceFiles);
        }
        if (config.analysis.dynamic_enabled)
        {
            coretrace::log(coretrace::Level::Info, "Running dynamic analysis on {} file(s)\n",
                           sourceFiles.size());
            invoker.runDynamicTools(sourceFiles);
        }
        if (!config.analysis.invoke.empty())
        {
            coretrace::log(coretrace::Level::Info, "Running specific tools on {} file(s)\n",
                           sourceFiles.size());
            invoker.runSpecificTools(config.analysis.invoke, sourceFiles);
        }
        return 0;
    }
} // namespace ctrace
