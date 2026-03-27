#include "App/Config.hpp"
#include "App/ToolConfig.hpp"

#include "ArgumentParser/ArgumentManager.hpp"
#include "ArgumentParser/ArgumentParserFactory.hpp"

#include <cstdlib>
#include <iostream>

namespace ctrace
{
    CT_NODISCARD ProgramConfig buildConfig(int argc, char* argv[])
    {
        auto parser = createArgumentParser();
        ArgumentManager argManager(std::move(parser));
        ProgramConfig config;

        // TODO : lvl verbosity
        argManager.addOption("--verbose", false, 'v');
        argManager.addFlag("--help", 'h');
        argManager.addFlag("--version", 'V');
        argManager.addFlag("--quiet", 'q');
        argManager.addOption("--output-file", true, 'o');
        argManager.addOption("--invoke", true, 'i');
        argManager.addOption("--sarif-format", false, 'f');
        argManager.addOption("--input", true, 's');
        argManager.addOption("--static", false, 'x');
        argManager.addOption("--dyn", false, 'd');
        argManager.addOption("--entry-points", true, 'e');
        argManager.addOption("--config", true, 'j');
        argManager.addOption("--compile-commands", true, 'c');
        argManager.addOption("--include-compdb-deps", false, 'u');
        argManager.addOption("--analysis-profile", true, 'P');
        argManager.addOption("--smt", true, 'S');
        argManager.addOption("--smt-backend", true, 'N');
        argManager.addOption("--smt-secondary-backend", true, 'W');
        argManager.addOption("--smt-mode", true, 'M');
        argManager.addOption("--smt-timeout-ms", true, 'T');
        argManager.addOption("--smt-budget-nodes", true, 'G');
        argManager.addOption("--smt-rules", true, 'U');
        argManager.addOption("--resource-model", true, 'R');
        argManager.addOption("--escape-model", true, 'E');
        argManager.addOption("--buffer-model", true, 'B');
        argManager.addOption("--timing", false, 'H');
        argManager.addOption("--demangle", false, 'g');
        argManager.addOption("--stack-limit", true, 'l');
        argManager.addOption("--report-file", true, 'r');
        argManager.addOption("--async", false, 'a');
        argManager.addOption("--ipc", true, 'p');
        argManager.addOption("--ipc-path", true, 't');
        argManager.addOption("--serve-host", true, 'z');
        argManager.addOption("--serve-port", true, 'y');
        argManager.addOption("--shutdown-token", true, 'k');
        argManager.addOption("--shutdown-timeout-ms", true, 'm');

        // Parsing des arguments
        argManager.parse(argc, argv);

        if (argManager.hasNoOption())
        {
            printHelp();
            std::exit(0);
        }

        if (argManager.hasOption("--help"))
        {
            printHelp();
            std::exit(0);
        }

        if (argManager.hasOption("--version"))
        {
            printVersion();
            std::exit(0);
        }

        // Traitement avec Command
        ConfigProcessor processor(config);
        if (argManager.hasOption("--config"))
        {
            const auto configPath = argManager.getOptionValue("--config");
            std::string toolConfigError;
            if (!ctrace::applyToolConfigFile(config, configPath, toolConfigError))
            {
                std::cerr << "Error: failed to load config '" << configPath
                          << "': " << toolConfigError << std::endl;
                std::exit(EXIT_FAILURE);
            }
        }
        processor.process(argManager);
        // processor.execute(argManager);

        if (!(argManager.getOptionValue("--ipc") == "serve") &&
            (argManager.hasOption("--serve-host") || argManager.hasOption("--serve-port")))
        {
            std::cout << "[INFO] UNCONSISTENT SERVER OPTIONS: --serve-host or --serve-port needed "
                         "--ipc=server."
                      << std::endl;
            std::exit(1);
        }

        return config;
    }
} // namespace ctrace
