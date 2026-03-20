#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <future>
#include <functional>
#include <optional>
#include <unordered_map>
#include "ArgumentParser/ArgumentManager.hpp"
#include "ArgumentParser/ArgumentParserFactory.hpp"
#include "App/SupportedTools.hpp"
#include "ctrace_tools/strings.hpp"
#include "ctrace_defs/types.hpp"

#include <coretrace/logger.hpp>

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
  --config <path>          Loads settings from a JSON config file.
  --compile-commands <path> Path to compile_commands.json for tools that support it.
  --include-compdb-deps    Includes dependency entries (e.g. _deps) when auto-loading files from compile_commands.json.
  --analysis-profile <p>   Stack analyzer profile: fast|full.
  --smt <on|off>           Enables/disables SMT refinement in stack analyzer.
  --smt-backend <name>     Primary SMT backend (e.g. z3, interval).
  --smt-secondary-backend <name> Secondary backend for multi-solver modes.
  --smt-mode <mode>        SMT mode: single|portfolio|cross-check|dual-consensus.
  --smt-timeout-ms <n>     SMT timeout in milliseconds.
  --smt-budget-nodes <n>   SMT node budget per query.
  --smt-rules <list>       Comma-separated SMT-enabled rules.
  --resource-model <path>  Path to the resource lifetime model for stack analyzer.
  --escape-model <path>    Path to the stack escape model for stack analyzer.
  --buffer-model <path>    Path to the buffer overflow model for stack analyzer.
  --demangle               Displays demangled function names in supported tools.
  --static                 Enables static analysis.
  --dyn                    Enables dynamic analysis.
  --invoke <tools>         Invokes specific tools (comma-separated).
                           Available tools: flawfinder, ikos, cppcheck, tscancode, ctrace_stack_analyzer.
  --input <files>          Specifies the source files to analyse (comma-separated).
  --timing                 Enables stack analyzer timing output.
  --ipc <method>           Specifies the IPC method to use (e.g., fifo, socket).
  --ipc-path <path>        Specifies the IPC path (default: /tmp/coretrace_ipc).
  --serve-host <host>      HTTP server host when --ipc=serve.
  --serve-port <port>      HTTP server port when --ipc=serve.
  --async                  Enables asynchronous execution.
  --shutdown-token <tok>   Token required for POST /shutdown (server mode).
  --shutdown-timeout-ms <ms> Graceful shutdown timeout in ms (0 = wait indefinitely).

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
        std::string src_file; ///< Path to the source file.

        explicit FileConfig(std::string_view sv) : src_file(sv) {}
        explicit FileConfig(const std::string& str) : src_file(str) {}
    };

    struct SpecificConfig
    {
        std::string tool_name; ///< Name of the specific tool.
        bool timing = false;   ///< Indicates if timing information should be displayed.
    };

    /**
     * @brief Represents the global configuration for the program.
     *
     * The `GlobalConfig` struct stores various global settings, such as verbosity,
     * analysis options, and file paths for reports and outputs.
     */
    struct GlobalConfig : public SpecificConfig
    {
        bool verbose = false;  ///< Enables verbose output.
        bool quiet = false;    ///< Suppresses non-essential output.
        bool demangle = false; ///< Enables demangled function names in supported tools.
        std::launch hasAsync = std::launch::deferred; ///< Enables asynchronous execution.
        bool hasSarifFormat = false;                  ///< Indicates if SARIF format is enabled.
        bool hasStaticAnalysis = false;               ///< Indicates if static analysis is enabled.
        bool hasDynamicAnalysis = false;              ///< Indicates if dynamic analysis is enabled.
        bool hasInvokedSpecificTools = false;         ///< Indicates if specific tools are invoked.
        std::string ipc =
            ctrace_defs::IPC_TYPES.front();         ///< IPC method to use (e.g., fifo, socket).
        std::string ipcPath = "/tmp/coretrace_ipc"; ///< Path for IPC communication.
        std::string serverHost = "127.0.0.1";       ///< Host for server IPC (if applicable).
        int serverPort = 8080;                      ///< Port for server IPC (if applicable).
        std::string shutdownToken;                  ///< Token required for POST /shutdown.
        int shutdownTimeoutMs = 0; ///< Shutdown timeout in milliseconds (0 = wait indefinitely).

        std::vector<std::string> specificTools; ///< List of specific tools to invoke.

        std::string entry_points = "main";             ///< Entry points for analysis.
        std::string report_file = "ctrace-report.txt"; ///< Path to the report file.
        std::string output_file = "ctrace.out";        ///< Path to the output file.
        std::string config_file;                       ///< Path to the JSON config file.
        std::string compile_commands;                  ///< Path to compile_commands.json.
        bool include_compdb_deps =
            false; ///< Includes dependency entries from compile_commands.json auto-discovery.
        std::string analysis_profile;       ///< Stack analyzer analysis profile (fast|full).
        std::string smt;                    ///< SMT enable switch (on|off).
        std::string smt_backend;            ///< SMT primary backend.
        std::string smt_secondary_backend;  ///< SMT secondary backend.
        std::string smt_mode;               ///< SMT mode.
        uint32_t smt_timeout_ms = 0;        ///< SMT timeout in milliseconds (0 = analyzer default).
        uint64_t smt_budget_nodes = 0;      ///< SMT node budget (0 = analyzer default).
        std::vector<std::string> smt_rules; ///< SMT-enabled rule ids.
        std::string resource_model;         ///< Path to stack analyzer resource model.
        std::string escape_model;           ///< Path to stack analyzer escape model.
        std::string buffer_model;           ///< Path to stack analyzer buffer model.
        std::string stack_analyzer_mode = "ir"; ///< Stack analyzer execution mode.
        std::string stack_analyzer_output_format; ///< Stack analyzer output format.
        std::string stack_analyzer_config; ///< Optional analyzer-native key=value config path.
        bool stack_analyzer_print_effective_config = false; ///< Print analyzer effective config.
        bool stack_analyzer_compdb_fast = false; ///< Enables fast compile DB mode in analyzer.
        bool stack_analyzer_include_stl = false; ///< Include STL/system functions in analyzer.
        bool stack_analyzer_dump_filter = false; ///< Enables analyzer filter tracing.
        bool stack_analyzer_warnings_only = false; ///< Emit warning/error diagnostics only.
        bool stack_analyzer_resource_summary_cache_memory_only =
            false; ///< Keep resource summary cache in memory only.
        std::optional<bool>
            stack_analyzer_resource_cross_tu; ///< Override analyzer resource cross-TU toggle.
        std::optional<bool>
            stack_analyzer_uninitialized_cross_tu; ///< Override uninitialized cross-TU toggle.
        std::string stack_analyzer_jobs; ///< Analyzer jobs value ("auto" or positive integer).
        std::string stack_analyzer_base_dir; ///< Base directory for SARIF URI normalization.
        std::string stack_analyzer_dump_ir; ///< Dump LLVM IR path (file/dir).
        std::string
            stack_analyzer_resource_summary_cache_dir; ///< Resource summary cache directory.
        std::string stack_analyzer_compile_ir_cache_dir; ///< Compile IR cache directory.
        std::string stack_analyzer_compile_ir_format; ///< Compile IR format (bc|ll).
        std::vector<std::string> stack_analyzer_only_files; ///< --only-file filters.
        std::vector<std::string> stack_analyzer_only_dirs; ///< --only-dir filters.
        std::vector<std::string> stack_analyzer_exclude_dirs; ///< --exclude-dir filters.
        std::vector<std::string> stack_analyzer_only_functions; ///< --only-func filters.
        std::vector<std::string> stack_analyzer_include_dirs; ///< -I include directories.
        std::vector<std::string> stack_analyzer_defines; ///< -D preprocessor defines.
        std::vector<std::string> stack_analyzer_compile_args; ///< --compile-arg values.
        std::vector<std::string> stack_analyzer_extra_args; ///< Extra stack analyzer args.
        uint64_t stack_limit = 8 * 1024 * 1024; ///< Stack limit in bytes.
    };

    /**
     * @brief Represents the overall program configuration.
     *
     * The `ProgramConfig` struct combines the global configuration and a list
     * of file-specific configurations.
     */
    struct ProgramConfig
    {
        GlobalConfig global;           ///< Global configuration settings.
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
    class ConfigProcessor
    {
      public:
        /**
             * @brief Constructs a `ConfigProcessor` with a reference to the program configuration.
             *
             * @param cfg The program configuration to update.
             */
        ConfigProcessor(ProgramConfig& cfg) : config(cfg)
        {
            // Initialize command mappings.
            commands["--help"] = [this](const std::string&)
            {
                printHelp();
                std::exit(0);
            };
            commands["--verbose"] = [this](const std::string&) { config.global.verbose = true; };
            commands["--quiet"] = [this](const std::string&) { config.global.quiet = true; };
            commands["--demangle"] = [this](const std::string&) { config.global.demangle = true; };
            commands["--sarif-format"] = [this](const std::string&)
            { config.global.hasSarifFormat = true; };
            commands["--report-file"] = [this](const std::string& value)
            { config.global.report_file = value; };
            commands["--output-file"] = [this](const std::string& value)
            { config.global.output_file = value; };
            commands["--async"] = [this](const std::string&)
            {
                config.global.hasAsync = std::launch::async;
                std::cout << "Asynchronous execution enabled." << std::endl;
            };
            commands["--invoke"] = [this](const std::string& value)
            {
                std::vector<std::string> parts;
                for (const auto part : ctrace_tools::strings::splitByComma(value))
                {
                    parts.emplace_back(part);
                }
                std::string normalizeError;
                const auto normalized = normalizeAndValidateToolList(parts, normalizeError);
                if (!normalizeError.empty())
                {
                    std::cerr << normalizeError << std::endl;
                    std::exit(EXIT_FAILURE);
                }
                config.global.specificTools = normalized;
                config.global.hasInvokedSpecificTools = !normalized.empty();
            };
            commands["--input"] = [this](const std::string& value) { config.addFile(value); };
            commands["--static"] = [this](const std::string&)
            { config.global.hasStaticAnalysis = true; };
            commands["--dyn"] = [this](const std::string&)
            { config.global.hasDynamicAnalysis = true; };
            commands["--entry-points"] = [this](const std::string& value)
            { config.global.entry_points = value; };
            commands["--config"] = [this](const std::string& value)
            { config.global.config_file = value; };
            commands["--compile-commands"] = [this](const std::string& value)
            { config.global.compile_commands = value; };
            commands["--include-compdb-deps"] = [this](const std::string&)
            { config.global.include_compdb_deps = true; };
            commands["--analysis-profile"] = [this](const std::string& value)
            { config.global.analysis_profile = value; };
            commands["--smt"] = [this](const std::string& value) { config.global.smt = value; };
            commands["--smt-backend"] = [this](const std::string& value)
            { config.global.smt_backend = value; };
            commands["--smt-secondary-backend"] = [this](const std::string& value)
            { config.global.smt_secondary_backend = value; };
            commands["--smt-mode"] = [this](const std::string& value)
            { config.global.smt_mode = value; };
            commands["--smt-timeout-ms"] = [this](const std::string& value)
            {
                try
                {
                    config.global.smt_timeout_ms = static_cast<uint32_t>(std::stoul(value));
                }
                catch (const std::exception& e)
                {
                    coretrace::log(coretrace::Level::Error,
                                   "Invalid smt timeout value: '{}'. Error: {}\n", value, e.what());
                    std::exit(EXIT_FAILURE);
                }
            };
            commands["--smt-budget-nodes"] = [this](const std::string& value)
            {
                try
                {
                    config.global.smt_budget_nodes = std::stoull(value);
                }
                catch (const std::exception& e)
                {
                    coretrace::log(coretrace::Level::Error,
                                   "Invalid smt budget value: '{}'. Error: {}\n", value, e.what());
                    std::exit(EXIT_FAILURE);
                }
            };
            commands["--smt-rules"] = [this](const std::string& value)
            {
                config.global.smt_rules.clear();
                for (const auto rule : ctrace_tools::strings::splitByComma(value))
                {
                    config.global.smt_rules.emplace_back(rule);
                }
            };
            commands["--resource-model"] = [this](const std::string& value)
            { config.global.resource_model = value; };
            commands["--escape-model"] = [this](const std::string& value)
            { config.global.escape_model = value; };
            commands["--buffer-model"] = [this](const std::string& value)
            { config.global.buffer_model = value; };
            commands["--timing"] = [this](const std::string&)
            { config.global.timing = true; };
            commands["--stack-limit"] = [this](const std::string& value)
            {
                try
                {
                    config.global.stack_limit = std::stoul(value);
                    coretrace::log(coretrace::Level::Info, "Stack limit set to {} bytes",
                                   config.global.stack_limit);
                }
                catch (const std::exception& e)
                {
                    coretrace::log(coretrace::Level::Error,
                                   "Invalid stack limit value: '{}'. Error: {}\n", value, e.what());
                    coretrace::log(coretrace::Level::Error,
                                   "Please provide a valid unsigned integer.\n");
                    std::exit(EXIT_FAILURE);
                }
            };
            commands["--ipc"] = [this](const std::string& value)
            {
                auto ipc_list = ctrace_defs::IPC_TYPES;

                if (std::find(ipc_list.begin(), ipc_list.end(), value) == ipc_list.end())
                {
                    std::cerr << "Invalid IPC type: '" << value << "'\n"
                              << "Available IPC types: [";
                    for (const auto& ipc : ipc_list)
                    {
                        std::cerr << ipc;
                        if (ipc != ipc_list.back())
                            std::cerr << ", ";
                    }
                    std::cerr << "]" << std::endl;
                    std::exit(EXIT_FAILURE);
                }
                config.global.ipc = value;
            };
            commands["--ipc-path"] = [this](const std::string& value)
            { config.global.ipcPath = value; };
            commands["--serve-host"] = [this](const std::string& value)
            {
                config.global.serverHost = value;
                coretrace::log(coretrace::Level::Debug, "Server host set to {}",
                               config.global.serverHost);
            };
            commands["--serve-port"] = [this](const std::string& value)
            {
                config.global.serverPort = std::stoi(value);
                coretrace::log(coretrace::Level::Debug, "Server port set to {}",
                               config.global.serverPort);
            };
            commands["--shutdown-token"] = [this](const std::string& value)
            { config.global.shutdownToken = value; };
            commands["--shutdown-timeout-ms"] = [this](const std::string& value)
            {
                config.global.shutdownTimeoutMs = std::stoi(value);
                if (config.global.shutdownTimeoutMs < 0)
                {
                    config.global.shutdownTimeoutMs = 0;
                }
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
        ProgramConfig& config; ///< Reference to the program configuration.
        std::unordered_map<std::string, std::function<void(const std::string&)>>
            commands; ///< Command map.
    };
} // namespace ctrace

#endif // CONFIG_HPP
