#include "App/Config.hpp"

#include "ArgumentParser/ArgumentManager.hpp"
#include "ArgumentParser/ArgumentParserFactory.hpp"

#include <cstdlib>
#include <iostream>

namespace ctrace
{
    CT_NODISCARD ProgramConfig buildConfig(int argc, char *argv[])
    {
        auto parser = createArgumentParser();
        ArgumentManager argManager(std::move(parser));
        ProgramConfig config;

        // TODO : lvl verbosity
        argManager.addOption("--verbose", false, 'v');
        argManager.addFlag("--help", 'h');
        argManager.addOption("--output", true, 'o');
        argManager.addOption("--invoke", true, 'i');
        argManager.addOption("--sarif-format", false, 'f');
        argManager.addOption("--input", true, 's');
        argManager.addOption("--static", false, 'x');
        argManager.addOption("--dyn", false, 'd');
        argManager.addOption("--entry-points", true, 'e');
        argManager.addOption("--report-file", true, 'r');
        argManager.addOption("--async", false, 'a');
        argManager.addOption("--ipc", true, 'p');
        argManager.addOption("--ipc-path", true, 't');
        argManager.addOption("--serve-host", true, 'z');
        argManager.addOption("--serve-port", true, 'y');

        // Parsing des arguments
        argManager.parse(argc, argv);

        // Traitement avec Command
        ConfigProcessor processor(config);
        processor.process(argManager);
        // processor.execute(argManager);

        if (argManager.hasNoOption())
        {
            processor.execute("--help", "");
            std::exit(0);
        }
        if (!(argManager.getOptionValue("--ipc") == "serve")
        && (argManager.hasOption("--serve-host")
        || argManager.hasOption("--serve-port"))
        )
        {
            std::cout << "[INFO] UNCONSISTENT SERVER OPTIONS: --serve-host or --serve-port needed --ipc=server."
                      << std::endl;
            std::exit(1);
        }

        return config;
    }
}
