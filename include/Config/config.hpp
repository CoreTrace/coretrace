// SPDX-License-Identifier: Apache-2.0
#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ctrace_defs/types.hpp"
#include "ctrace_tools/strings.hpp"

namespace ctrace
{
    // The configuration mirrors docs/configuration.md section by section. Every input path
    // (config file, HTTP params, CLI) fills the same structure; see ROADMAP.md M1/M2.

    struct AnalysisConfig
    {
        bool static_enabled = false;     ///< `analysis.static`: run the static tool set.
        bool dynamic_enabled = false;    ///< `analysis.dynamic`: run the dynamic tool set.
        std::vector<std::string> invoke; ///< `analysis.invoke`: explicit tool selection.
    };

    struct FilesConfig
    {
        std::vector<std::string> input;        ///< Source files and/or manifests to analyse.
        std::vector<std::string> entry_points; ///< Entry-point filter forwarded to tools.
        std::string compile_commands;          ///< Path to compile_commands.json or its directory.
        bool include_compdb_deps = false;      ///< Keep `_deps` entries from a compile database.
    };

    struct OutputConfig
    {
        bool sarif_format = false;
        std::string report_file = "ctrace-report.txt";
        std::string output_file = "ctrace.out";
        bool verbose = false;
        bool quiet = false;
        bool demangle = false;
    };

    struct RuntimeConfig
    {
        bool async = false;                               ///< Thread-pool tool scheduling.
        std::string ipc = ctrace_defs::IPC_TYPES.front(); ///< standardIO|socket|serve.
        std::string ipc_path = "/tmp/coretrace_ipc";
    };

    struct ServerConfig
    {
        std::string host = "127.0.0.1";
        int port = 8080;
        std::string shutdown_token;
        int shutdown_timeout_ms = 0; ///< 0 = wait indefinitely.
    };

    /// Settings forwarded to coretrace-stack-analyzer. Empty strings and zero mean "keep the
    /// analyzer default"; std::optional distinguishes an explicit false from "not set".
    struct StackAnalyzerConfig
    {
        std::string mode = "ir";
        std::string output_format;
        std::string config; ///< Analyzer-native key=value config path.
        bool print_effective_config = false;
        bool compdb_fast = false;
        std::string jobs; ///< "auto" or a positive integer.
        std::vector<std::string> include_dirs;
        std::vector<std::string> defines;
        std::vector<std::string> compile_args;
        std::vector<std::string> only_functions;
        std::vector<std::string> only_files;
        std::vector<std::string> only_dirs;
        std::vector<std::string> exclude_dirs;
        std::string analysis_profile; ///< fast|full.
        std::string smt;              ///< on|off.
        std::string smt_backend;
        std::string smt_secondary_backend;
        std::string smt_mode;
        std::uint32_t smt_timeout_ms = 0;
        std::uint64_t smt_budget_nodes = 0;
        std::vector<std::string> smt_rules;
        std::string resource_model;
        std::string escape_model;
        std::string buffer_model;
        std::optional<bool> resource_cross_tu;
        std::optional<bool> uninitialized_cross_tu;
        std::string resource_summary_cache_dir;
        bool resource_summary_cache_memory_only = false;
        std::string compile_ir_cache_dir;
        std::string compile_ir_format; ///< bc|ll.
        bool include_stl = false;
        std::uint64_t stack_limit = 8ULL * 1024ULL * 1024ULL;
        std::string base_dir; ///< SARIF URI normalization base.
        std::string dump_ir;
        bool dump_filter = false;
        bool warnings_only = false;
        bool timing = false;
        std::vector<std::string> extra_args; ///< Forwarded verbatim after mapped options.
    };

    /// Where external tools live. A value may be a bare command name, resolved through PATH,
    /// or an absolute path. Tools absent from the map use their own name as the command.
    struct ToolsConfig
    {
        std::map<std::string, std::string> paths;
    };

    struct ProgramConfig
    {
        AnalysisConfig analysis;
        FilesConfig files;
        OutputConfig output;
        RuntimeConfig runtime;
        ServerConfig server;
        StackAnalyzerConfig stack_analyzer;
        ToolsConfig tools;
        std::string config_file; ///< Path of the loaded JSON config, empty when none.

        /// Splits a comma-separated list of paths into `files.input`.
        void addFile(const std::string& csv)
        {
            for (const auto part : ctrace_tools::strings::splitByComma(csv))
            {
                files.input.emplace_back(part);
            }
        }
    };
} // namespace ctrace

#endif // CONFIG_HPP
