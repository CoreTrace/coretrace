// SPDX-License-Identifier: Apache-2.0
#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ctrace_defs/types.hpp"
#include "ctrace_tools/strings.hpp"

namespace ctrace
{
    // The configuration mirrors docs/configuration.md section by section. Every input path
    // (config file, HTTP params, CLI) fills the same structure; see ROADMAP.md M1/M2.

    /// Lowest severity that makes the run exit non-zero (`analysis.fail_on`, `--fail-on`).
    enum class FailOn
    {
        None,
        Warning,
        Error
    };

    inline constexpr std::array<std::pair<std::string_view, FailOn>, 3> kFailOnValues = {{
        {"none", FailOn::None},
        {"warning", FailOn::Warning},
        {"error", FailOn::Error},
    }};

    [[nodiscard]] constexpr std::optional<FailOn> parseFailOn(std::string_view name) noexcept
    {
        for (const auto& [candidate, value] : kFailOnValues)
        {
            if (candidate == name)
            {
                return value;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] constexpr std::string_view failOnName(FailOn policy) noexcept
    {
        for (const auto& [name, value] : kFailOnValues)
        {
            if (value == policy)
            {
                return name;
            }
        }
        return "error";
    }

    struct AnalysisConfig
    {
        bool static_enabled = false;     ///< `analysis.static`: run the static tool set.
        bool dynamic_enabled = false;    ///< `analysis.dynamic`: run the dynamic tool set.
        std::vector<std::string> invoke; ///< `analysis.invoke`: explicit tool selection.
        FailOn fail_on = FailOn::Error;  ///< `analysis.fail_on`: exit-code gate policy.
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
        /// The largest `jobs` accepted: each one is a thread.
        static constexpr std::uint64_t kMaxJobs = 1024;
        /// `-j/--jobs`: how many tool runs go on at the same time; 0 for one per core, 1 for a
        /// sequential run. `runtime.async` and `--async` are deprecated spellings.
        std::uint64_t jobs = 0;
        std::string ipc = ctrace_defs::IPC_TYPES.front(); ///< standardIO|socket|serve.
        std::string ipc_path = "/tmp/coretrace_ipc";
    };

    struct ServerConfig
    {
        std::string host = "127.0.0.1";
        int port = 8080;
        std::string shutdown_token;
        int shutdown_timeout_ms = 0; ///< 0 = wait indefinitely.

        /// Largest accepted request body. Requests carry a file list, not file contents.
        std::uint64_t max_body_bytes = 1024ULL * 1024ULL;

        /// Origin allowed to call the API from a browser. Empty means no cross-origin header
        /// is sent, so a browser on another origin cannot read the response.
        std::string cors_origin;

        /// Whether requests may run dynamic analysis, which builds and runs the client's code
        /// on this machine with the server's privileges. Off unless the server is started so.
        bool allow_dynamic_analysis = false;
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
        std::string assume_external_frame; ///< Bytes or size with KiB/MiB/GiB suffix.
        std::string base_dir;              ///< SARIF URI normalization base.
        std::string dump_ir;
        bool dump_filter = false;
        bool warnings_only = false;
        bool timing = false;
        std::vector<std::string> extra_args; ///< Forwarded verbatim after mapped options.
    };

    /// How the project builds, for every C/C++ tool that compiles or preprocesses the inputs:
    /// each include directory becomes `-I<dir>`, each macro `-D<macro>`.
    struct BuildConfig
    {
        std::vector<std::string> include_dirs;
        std::vector<std::string> defines;
    };

    /// Where external tools live and what extra arguments they get. A path may be a bare
    /// command name, resolved through PATH, or an absolute path; tools absent from the map use
    /// their own name. `args` are appended verbatim after the options CoreTrace derives.
    struct ToolsConfig
    {
        std::map<std::string, std::string> paths;
        std::map<std::string, std::vector<std::string>> args;
        /// The timeout of an external tool without `tools.<name>.timeout_s`.
        static constexpr std::uint32_t kDefaultTimeoutSeconds = 600;
        /// `tools.<name>.timeout_s` of the external tools but the runtime analyzer: how long one
        /// run may take before it is stopped and reported as failed; 0 disables the limit.
        std::map<std::string, std::uint32_t> timeouts_s;
        std::uint32_t cppcheck_jobs = 0; ///< Zero keeps cppcheck's default.
        std::vector<std::string> runtime_analyzer_compile_args;
        std::size_t concurrency_analyzer_max_live_units = 0; ///< Zero keeps analyzer default.
        /// `tools.coretrace-runtime-analyzer.timeout_s`: how long each program may run; 0
        /// disables the limit.
        std::uint32_t runtime_analyzer_timeout_s = 60;
        /// `tools.coretrace-concurrency-analyzer.rules`: the rules to run, by the names the
        /// analyzer's command line uses (data-race, missing-join...); empty means every rule.
        std::vector<std::string> concurrency_analyzer_rules;
    };

    struct ProgramConfig
    {
        AnalysisConfig analysis;
        FilesConfig files;
        OutputConfig output;
        RuntimeConfig runtime;
        ServerConfig server;
        StackAnalyzerConfig stack_analyzer;
        BuildConfig build;
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
