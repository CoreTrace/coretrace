
#include "ArgumentParser/ArgumentManager.hpp"
#include "ArgumentParser/ArgumentParserFactory.hpp"
#include "Process/ProcessFactory.hpp"
#include "Process/Tools/ToolsInvoker.hpp"
#include "Config/config.hpp"

#include "ctrace_tools/languageType.hpp"
#include "ctrace_tools/colors.hpp"

#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

#include <cxxabi.h>
#include <string_view>

using json = nlohmann::json;


namespace ctrace
{
    ProgramConfig buildConfig(int argc, char *argv[])
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

        // Parsing des arguments
        argManager.parse(argc, argv);

        // Traitement avec Command
        ConfigProcessor processor(config);
        processor.process(argManager);
        // processor.execute(argManager);

        return config;
    }
}

int main(int argc, char *argv[])
{
    ctrace::ProgramConfig config = ctrace::buildConfig(argc, argv);
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

    // TODO : handle multi-file parsing
    for (const auto& file : config.files)
    {
        std::cout << ctrace::Color::CYAN << "File: "
                  << ctrace::Color::YELLOW << file.src_file << ctrace::Color::RESET << std::endl;

        if (config.global.hasStaticAnalysis)
        {
            std::cout << ctrace::Color::CYAN << "Running static analysis..." << ctrace::Color::RESET << std::endl;
            invoker.runStaticTools(file.src_file);
        }
        if (config.global.hasDynamicAnalysis)
        {
            std::cout << ctrace::Color::CYAN << "Running dynamic analysis..." << ctrace::Color::RESET << std::endl;
            invoker.runDynamicTools(file.src_file);
        }
        if (config.global.hasInvokedSpecificTools)
        {
            std::cout << ctrace::Color::CYAN << "Running specific tools..." << ctrace::Color::RESET << std::endl;
            invoker.runSpecificTools(config.global.specificTools, file.src_file);
        }
    }
    return 0;
}
