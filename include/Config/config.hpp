// SPDX-License-Identifier: Apache-2.0
#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <future>
#include <optional>
#include "App/SupportedTools.hpp"
#include "ctrace_tools/strings.hpp"
#include "ctrace_defs/types.hpp"

#include <coretrace/logger.hpp>

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

        std::string entry_points = "";                 ///< Entry points for analysis.
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
        std::string stack_analyzer_mode = "ir";   ///< Stack analyzer execution mode.
        std::string stack_analyzer_output_format; ///< Stack analyzer output format.
        std::string stack_analyzer_config; ///< Optional analyzer-native key=value config path.
        bool stack_analyzer_print_effective_config = false; ///< Print analyzer effective config.
        bool stack_analyzer_compdb_fast = false;   ///< Enables fast compile DB mode in analyzer.
        bool stack_analyzer_include_stl = false;   ///< Include STL/system functions in analyzer.
        bool stack_analyzer_dump_filter = false;   ///< Enables analyzer filter tracing.
        bool stack_analyzer_warnings_only = false; ///< Emit warning/error diagnostics only.
        bool stack_analyzer_resource_summary_cache_memory_only =
            false; ///< Keep resource summary cache in memory only.
        std::optional<bool>
            stack_analyzer_resource_cross_tu; ///< Override analyzer resource cross-TU toggle.
        std::optional<bool>
            stack_analyzer_uninitialized_cross_tu; ///< Override uninitialized cross-TU toggle.
        std::string stack_analyzer_jobs;     ///< Analyzer jobs value ("auto" or positive integer).
        std::string stack_analyzer_base_dir; ///< Base directory for SARIF URI normalization.
        std::string stack_analyzer_dump_ir;  ///< Dump LLVM IR path (file/dir).
        std::string
            stack_analyzer_resource_summary_cache_dir;        ///< Resource summary cache directory.
        std::string stack_analyzer_compile_ir_cache_dir;      ///< Compile IR cache directory.
        std::string stack_analyzer_compile_ir_format;         ///< Compile IR format (bc|ll).
        std::vector<std::string> stack_analyzer_only_files;   ///< --only-file filters.
        std::vector<std::string> stack_analyzer_only_dirs;    ///< --only-dir filters.
        std::vector<std::string> stack_analyzer_exclude_dirs; ///< --exclude-dir filters.
        std::vector<std::string> stack_analyzer_only_functions; ///< --only-func filters.
        std::vector<std::string> stack_analyzer_include_dirs;   ///< -I include directories.
        std::vector<std::string> stack_analyzer_defines;        ///< -D preprocessor defines.
        std::vector<std::string> stack_analyzer_compile_args;   ///< --compile-arg values.
        std::vector<std::string> stack_analyzer_extra_args;     ///< Extra stack analyzer args.
        uint64_t stack_limit = 8 * 1024 * 1024;                 ///< Stack limit in bytes.
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
} // namespace ctrace

#endif // CONFIG_HPP
