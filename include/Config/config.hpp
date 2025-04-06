#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include "ArgumentParser/ArgumentManager.hpp"
#include "ArgumentParser/ArgumentParserFactory.hpp"
#include "ctrace_tools/strings.hpp"

static void printHelp(void)
{
    std::cout << R"(ctrace - Static & Dynamic C/C++ Code Analysis Tool

Usage:
  ctrace [options]

Options:
  --help                   Displays this help message.
  --verbose                Enables detailed (verbose) output.
  --sarif-format           Generates a report in SARIF format.
  --report-file <path>     Specifies the path to the report file (default: ctrace-report.txt).
  --output-file <path>     Specifies the output file for the analysed binary (default: ctrace.out).
  --entry-points <names>   Sets the entry points for analysis (default: main). Accepts a comma-separated list.
  --static                 Enables static analysis.
  --dyn                    Enables dynamic analysis.
  --invoke <tools>         Invokes specific tools (comma-separated).
                           Available tools: flawfinder, ikos, cppcheck, tscancode.
  --input <files>          Specifies the source files to analyse (comma-separated).

Examples:
  ctrace --input main.cpp,util.cpp --static --invoke=cppcheck,flawfinder
  ctrace --verbose --report-file=analysis.txt --sarif-format

Description:
  ctrace is a modular C/C++ code analysis tool that combines both static and dynamic
  analysis. It can be finely configured to detect vulnerabilities, security issues,
  and memory misuse.
)" << std::endl;
}

namespace ctrace
{
    /**
     * @brief Represents the configuration for a single source file.
     *
     * The `FileConfig` struct stores the path to a source file. It provides
     * constructors to initialize the file path from either a `std::string_view`
     * or a `std::string`.
     */
    struct FileConfig
    {
        std::string src_file;///< Path to the source file.

        explicit FileConfig(std::string_view sv) : src_file(sv) {}
        explicit FileConfig(const std::string& str) : src_file(str) {}
    };

    /**
     * @brief Represents the global configuration for the program.
     *
     * The `GlobalConfig` struct stores various global settings, such as verbosity,
     * analysis options, and file paths for reports and outputs.
     */
    struct GlobalConfig
    {
        bool verbose = false; ///< Enables verbose output.
        bool hasSarifFormat = false; ///< Indicates if SARIF format is enabled.
        bool hasStaticAnalysis = false; ///< Indicates if static analysis is enabled.
        bool hasDynamicAnalysis = false; ///< Indicates if dynamic analysis is enabled.
        bool hasInvokedSpecificTools = false; ///< Indicates if specific tools are invoked.

        std::vector<std::string> specificTools; ///< List of specific tools to invoke.

        std::string entry_points = "main"; ///< Entry points for analysis.
        std::string report_file = "ctrace-report.txt"; ///< Path to the report file.
        std::string output_file = "ctrace.out"; ///< Path to the output file.
    };

    /**
     * @brief Represents the overall program configuration.
     *
     * The `ProgramConfig` struct combines the global configuration and a list
     * of file-specific configurations.
     */
    struct ProgramConfig
    {
        GlobalConfig global; ///< Global configuration settings.
        std::vector<FileConfig> files; ///< List of file-specific configurations.

        /**
         * @brief Adds a source file to the configuration.
         *
         * This function splits the input string by commas and adds each part
         * as a separate `FileConfig` to the `files` vector.
         *
         * @param src_file A comma-separated string of file paths.
         */
        void addFile(const std::string& src_file)
        {
            auto parts = ctrace_tools::strings::splitByComma(src_file);

            for (const auto& part : parts)
            {
                files.emplace_back(part);
            }
        }
    };

    /**
     * @brief Processes command-line arguments and updates the program configuration.
     *
     * The `ConfigProcessor` class maps command-line options to configuration
     * updates. It uses a command map to associate options with specific actions.
     */
    class ConfigProcessor {
        public:
            /**
             * @brief Constructs a `ConfigProcessor` with a reference to the program configuration.
             *
             * @param cfg The program configuration to update.
             */
            ConfigProcessor(ProgramConfig& cfg) : config(cfg) {
                // Initialize command mappings.
                commands["--help"] = [this](const std::string&)
                {
                    printHelp();
                    std::exit(0);
                };
                commands["--verbose"] = [this](const std::string&) { config.global.verbose = true; };
                commands["--sarif-format"] = [this](const std::string&) { config.global.hasSarifFormat = true; };
                commands["--report-file"] = [this](const std::string& value) { config.global.report_file = value; };
                commands["--invoke"] = [this](const std::string& value)
                {
                    config.global.hasInvokedSpecificTools = true;
                    auto parts = ctrace_tools::strings::splitByComma(value);

                    for (const auto& part : parts)
                    {
                        // TODO: Refactor this block for better readability.
                        std::cout << "Invoke tool: " << part << std::endl;
                        if (part == "flawfinder")
                        {
                            // config.global.hasStaticAnalysis = true;
                            std::cout << "Flawfinder invoked" << std::endl;
                            config.global.specificTools.emplace_back("flawfinder");
                        }
                        if (part == "ikos")
                        {
                            // config.global.hasStaticAnalysis = true;
                            std::cout << "Ikos invoked" << std::endl;
                            config.global.specificTools.emplace_back("ikos");
                        }
                        if (part == "cppcheck")
                        {
                            // config.global.hasStaticAnalysis = true;
                            std::cout << "Cppcheck invoked" << std::endl;
                            config.global.specificTools.emplace_back("cppcheck");
                        }
                        if (part == "tscancode")
                        {
                            // config.global.hasStaticAnalysis = true;
                            std::cout << "Tscancode invoked" << std::endl;
                            config.global.specificTools.emplace_back("tscancode");
                        }
                    }
                };
                commands["--input"] = [this](const std::string& value)
                {
                    config.addFile(value);
                };
                commands["--static"] = [this](const std::string&)
                {
                    config.global.hasStaticAnalysis = true;
                };
                commands["--dyn"] = [this](const std::string&)
                {
                    config.global.hasDynamicAnalysis = true;
                };
                commands["--entry-points"] = [this](const std::string& value)
                {
                    config.global.entry_points = value;
                };
                // commands["--output"] = [this](const std::string& value) {
                //     if (!config.files.empty()) config.files.back().output_file = value;
                // };
            }

            /**
             * @brief Executes a command based on the given option and value.
             *
             * @param option The command-line option.
             * @param value The value associated with the option.
             */
            void execute(const std::string& option, const std::string& value)
            {
                if (commands.count(option))
                {
                    commands[option](value);
                }
            }

            /**
             * @brief Processes all options from the argument manager.
             *
             * This function iterates through the available commands and applies
             * them if the corresponding option is present in the argument manager.
             *
             * @param argManager The argument manager containing parsed options.
             */
            void process(ArgumentManager& argManager)
            {
                for (const auto& [option, command] : commands)
                {
                    if (argManager.hasOption(option))
                    {
                        command(argManager.getOptionValue(option));
                    }
                }
            }

        private:
            ProgramConfig& config;  ///< Reference to the program configuration.
            std::unordered_map<std::string, std::function<void(const std::string&)>> commands; ///< Command map.
    };
}

#endif // CONFIG_HPP
