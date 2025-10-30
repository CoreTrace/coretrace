
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
#include <filesystem>
#include <vector>

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
        argManager.addOption("--ipc", true, 'p');
        argManager.addOption("--ipc-path", true, 't');

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

    std::vector<std::string> sourceFiles;
    sourceFiles.reserve(config.files.size());

    const auto appendResolved = [&](const std::string& candidate,
                                    const std::filesystem::path& baseDir) -> bool
    {
        if (candidate.empty())
        {
            return false;
        }
        std::filesystem::path resolved(candidate);
        if (resolved.is_relative() && !baseDir.empty())
        {
            resolved = baseDir / resolved;
        }
        resolved = resolved.lexically_normal();
        sourceFiles.emplace_back(resolved.string());
        return true;
    };

    for (const auto& fileConfig : config.files)
    {
        const std::string& entry = fileConfig.src_file;

        bool expanded = false;
        if (!entry.empty() && (entry.ends_with(".json") || entry.ends_with(".JSON")))
        {
            std::ifstream manifestStream(entry);
            if (manifestStream.is_open())
            {
                std::ostringstream buffer;
                buffer << manifestStream.rdbuf();

                const auto manifest = json::parse(buffer.str(), nullptr, false);
                if (!manifest.is_discarded())
                {
                    const std::filesystem::path manifestDir = std::filesystem::path(entry).parent_path();

                    const auto appendFromArray = [&](const json& arr) -> bool
                    {
                        bool appended = false;
                        for (const auto& item : arr)
                        {
                            if (item.is_string())
                            {
                                appended |= appendResolved(item.get<std::string>(), manifestDir);
                            }
                            else if (item.is_object())
                            {
                                if (const auto it = item.find("file"); it != item.end() && it->is_string())
                                {
                                    appended |= appendResolved(it->get<std::string>(), manifestDir);
                                }
                                else if (const auto itSrc = item.find("src_file"); itSrc != item.end() && itSrc->is_string())
                                {
                                    appended |= appendResolved(itSrc->get<std::string>(), manifestDir);
                                }
                                else if (const auto itPath = item.find("path"); itPath != item.end() && itPath->is_string())
                                {
                                    appended |= appendResolved(itPath->get<std::string>(), manifestDir);
                                }
                            }
                        }
                        return appended;
                    };

                    if (manifest.is_array())
                    {
                        expanded = appendFromArray(manifest);
                    }
                    else if (manifest.is_object())
                    {
                        if (const auto it = manifest.find("files"); it != manifest.end() && it->is_array())
                        {
                            expanded = appendFromArray(*it);
                        }
                        else if (const auto it = manifest.find("sources"); it != manifest.end() && it->is_array())
                        {
                            expanded = appendFromArray(*it);
                        }
                        else if (const auto it = manifest.find("compile_commands"); it != manifest.end() && it->is_array())
                        {
                            expanded = appendFromArray(*it);
                        }
                        else if (const auto itFile = manifest.find("file"); itFile != manifest.end() && itFile->is_string())
                        {
                            expanded = appendResolved(itFile->get<std::string>(), manifestDir);
                        }
                        else if (const auto itSrc = manifest.find("src_file"); itSrc != manifest.end() && itSrc->is_string())
                        {
                            expanded = appendResolved(itSrc->get<std::string>(), manifestDir);
                        }
                        else if (const auto itPath = manifest.find("path"); itPath != manifest.end() && itPath->is_string())
                        {
                            expanded = appendResolved(itPath->get<std::string>(), manifestDir);
                        }
                    }
                }
            }
        }

        if (!expanded)
        {
            sourceFiles.emplace_back(entry);
        }
    }

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
