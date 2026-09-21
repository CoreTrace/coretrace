// SPDX-License-Identifier: Apache-2.0
#include "Process/Tools/AnalysisTools.hpp"
#include "app/AnalyzerApp.hpp"

#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

#include <coretrace/logger.hpp>

#include <mutex>

namespace
{
    constexpr std::string_view kStackAnalyzerModule = "stack_analyzer";

    // The analyzer runs in-process and its thread-safety across concurrent runs is not
    // documented (on-disk summary caches, LLVM global state). Server mode handles requests on
    // a thread pool, so runs are serialized process-wide until the analyzer proves otherwise.
    // Per-tool locks in ToolInvoker only cover one invoker, not concurrent requests.
    std::mutex& analyzerRunMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    struct AnalyzerArgBuildResult
    {
        std::vector<std::string> args;
        std::vector<std::string> bridgeReport;
    };

    void appendBridgeDecision(std::vector<std::string>& report, std::string_view option,
                              bool applied, std::string_view reason)
    {
        std::string line;
        line.reserve(option.size() + reason.size() + 24);
        line.append(applied ? "[applied] " : "[skipped] ");
        line.append(option);
        if (!reason.empty())
        {
            line.append(" - ");
            line.append(reason);
        }
        report.emplace_back(std::move(line));
    }

    [[nodiscard]] bool appendOptionValue(std::vector<std::string>& args, std::string_view option,
                                         const std::string& value)
    {
        if (value.empty())
        {
            return false;
        }
        args.emplace_back(option);
        args.push_back(value);
        return true;
    }

    void appendFlagOption(std::vector<std::string>& args, std::vector<std::string>& report,
                          std::string_view option, bool enabled, std::string_view enabledReason,
                          std::string_view disabledReason)
    {
        if (!enabled)
        {
            appendBridgeDecision(report, option, false, disabledReason);
            return;
        }
        args.emplace_back(option);
        appendBridgeDecision(report, option, true, enabledReason);
    }

    void appendValueOption(std::vector<std::string>& args, std::vector<std::string>& report,
                           std::string_view option, const std::string& value,
                           std::string_view emptyReason)
    {
        if (!appendOptionValue(args, option, value))
        {
            appendBridgeDecision(report, option, false, emptyReason);
            return;
        }
        const std::string reason = "value='" + value + "'";
        appendBridgeDecision(report, option, true, reason);
    }

    [[nodiscard]] std::string joinCsv(const std::vector<std::string>& items)
    {
        std::string joined;
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            if (i > 0)
            {
                joined.push_back(',');
            }
            joined += items[i];
        }
        return joined;
    }

    [[nodiscard]] AnalyzerArgBuildResult
    buildAnalyzerArgs(const std::vector<std::string>& inputFiles,
                      const ctrace::ProgramConfig& config)
    {
        AnalyzerArgBuildResult result;
        std::vector<std::string>& args = result.args;
        std::vector<std::string>& report = result.bridgeReport;

        args.reserve(inputFiles.size() + 96);
        report.reserve(96);

        const std::string analyzerMode =
            config.stack_analyzer.mode.empty() ? "ir" : config.stack_analyzer.mode;
        args.emplace_back("--mode=" + analyzerMode);
        appendBridgeDecision(report, "--mode", true, "value='" + analyzerMode + "'");
        if (!config.config_file.empty())
        {
            appendBridgeDecision(report, "config source", true,
                                 "path='" + config.config_file +
                                     "' loaded by coretrace and mapped to analyzer options");
        }
        else
        {
            appendBridgeDecision(report, "config source", false,
                                 "no --config provided to coretrace");
        }

        appendValueOption(args, report, "--config", config.stack_analyzer.config,
                          "empty stack_analyzer.config; analyzer internal config disabled");
        appendFlagOption(args, report, "--print-effective-config",
                         config.stack_analyzer.print_effective_config,
                         "stack_analyzer.print_effective_config enabled",
                         "stack_analyzer.print_effective_config disabled");

        if (!config.stack_analyzer.output_format.empty())
        {
            args.emplace_back("--format=" + config.stack_analyzer.output_format);
            appendBridgeDecision(report, "--format", true,
                                 "value='" + config.stack_analyzer.output_format + "'");
        }
        else
        {
            appendFlagOption(args, report, "--format=json", config.output.sarif_format,
                             "derived from coretrace --sarif-format",
                             "empty stack_analyzer.output_format and sarif disabled");
        }
        appendFlagOption(args, report, "--verbose", config.output.verbose,
                         "coretrace --verbose enabled", "coretrace --verbose disabled");
        appendFlagOption(args, report, "--demangle", config.output.demangle,
                         "coretrace --demangle enabled", "coretrace --demangle disabled");
        appendFlagOption(args, report, "--quiet", config.output.quiet, "coretrace --quiet enabled",
                         "coretrace --quiet disabled");
        appendFlagOption(args, report, "--include-compdb-deps", config.files.include_compdb_deps,
                         "coretrace --include-compdb-deps enabled",
                         "coretrace --include-compdb-deps disabled");
        appendFlagOption(args, report, "--compdb-fast", config.stack_analyzer.compdb_fast,
                         "stack_analyzer.compdb_fast enabled",
                         "stack_analyzer.compdb_fast disabled");
        appendFlagOption(args, report, "--STL", config.stack_analyzer.include_stl,
                         "stack_analyzer.include_stl enabled",
                         "stack_analyzer.include_stl disabled");
        appendFlagOption(args, report, "--warnings-only", config.stack_analyzer.warnings_only,
                         "stack_analyzer.warnings_only enabled",
                         "stack_analyzer.warnings_only disabled");
        appendFlagOption(args, report, "--dump-filter", config.stack_analyzer.dump_filter,
                         "stack_analyzer.dump_filter enabled",
                         "stack_analyzer.dump_filter disabled");

        appendValueOption(args, report, "--analysis-profile",
                          config.stack_analyzer.analysis_profile,
                          "empty in coretrace config; analyzer default kept");
        appendValueOption(args, report, "--compile-commands", config.files.compile_commands,
                          "empty in coretrace config; analyzer auto-discovery kept");
        if (!config.stack_analyzer.jobs.empty())
        {
            appendValueOption(args, report, "--jobs", config.stack_analyzer.jobs,
                              "empty stack_analyzer.jobs");
        }
        else
        {
            appendBridgeDecision(report, "--jobs", false,
                                 "empty stack_analyzer.jobs; analyzer default kept");
        }

        appendValueOption(args, report, "--resource-summary-cache-dir",
                          config.stack_analyzer.resource_summary_cache_dir,
                          "empty stack_analyzer.resource_summary_cache_dir");
        appendValueOption(args, report, "--compile-ir-cache-dir",
                          config.stack_analyzer.compile_ir_cache_dir,
                          "empty stack_analyzer.compile_ir_cache_dir");
        appendValueOption(args, report, "--compile-ir-format",
                          config.stack_analyzer.compile_ir_format,
                          "empty stack_analyzer.compile_ir_format");
        appendValueOption(args, report, "--dump-ir", config.stack_analyzer.dump_ir,
                          "empty stack_analyzer.dump_ir");
        appendValueOption(args, report, "--base-dir", config.stack_analyzer.base_dir,
                          "empty stack_analyzer.base_dir");
        appendValueOption(args, report, "--resource-model", config.stack_analyzer.resource_model,
                          "empty in coretrace config; analyzer default model");
        appendValueOption(args, report, "--escape-model", config.stack_analyzer.escape_model,
                          "empty in coretrace config; analyzer default model");
        appendValueOption(args, report, "--buffer-model", config.stack_analyzer.buffer_model,
                          "empty in coretrace config; analyzer default model");
        appendValueOption(args, report, "--smt", config.stack_analyzer.smt,
                          "empty in coretrace config; analyzer default SMT");
        appendValueOption(args, report, "--smt-backend", config.stack_analyzer.smt_backend,
                          "empty in coretrace config; analyzer default backend");
        appendValueOption(args, report, "--smt-secondary-backend",
                          config.stack_analyzer.smt_secondary_backend,
                          "empty in coretrace config; analyzer default secondary backend");
        appendValueOption(args, report, "--smt-mode", config.stack_analyzer.smt_mode,
                          "empty in coretrace config; analyzer default mode");

        if (!config.stack_analyzer.only_files.empty())
        {
            for (const auto& filter : config.stack_analyzer.only_files)
            {
                appendValueOption(args, report, "--only-file", filter,
                                  "empty value in stack_analyzer.only_files");
            }
        }
        else
        {
            appendBridgeDecision(report, "--only-file", false, "empty stack_analyzer.only_files");
        }

        if (!config.stack_analyzer.only_dirs.empty())
        {
            for (const auto& filter : config.stack_analyzer.only_dirs)
            {
                appendValueOption(args, report, "--only-dir", filter,
                                  "empty value in stack_analyzer.only_dirs");
            }
        }
        else
        {
            appendBridgeDecision(report, "--only-dir", false, "empty stack_analyzer.only_dirs");
        }

        if (!config.stack_analyzer.exclude_dirs.empty())
        {
            for (const auto& filter : config.stack_analyzer.exclude_dirs)
            {
                appendValueOption(args, report, "--exclude-dir", filter,
                                  "empty value in stack_analyzer.exclude_dirs");
            }
        }
        else
        {
            appendBridgeDecision(report, "--exclude-dir", false,
                                 "empty stack_analyzer.exclude_dirs");
        }

        if (!config.stack_analyzer.only_functions.empty())
        {
            args.emplace_back("--only-func");
            args.emplace_back(joinCsv(config.stack_analyzer.only_functions));
            appendBridgeDecision(report, "--only-func", true,
                                 "value='" + joinCsv(config.stack_analyzer.only_functions) + "'");
        }
        else
        {
            appendValueOption(args, report, "--only-function",
                              ctrace_tools::strings::joinByComma(config.files.entry_points),
                              "empty in coretrace config; no entry-point filter");
        }

        if (!config.stack_analyzer.include_dirs.empty())
        {
            for (const auto& includeDir : config.stack_analyzer.include_dirs)
            {
                if (includeDir.empty())
                {
                    appendBridgeDecision(report, "-I", false,
                                         "empty value in stack_analyzer.include_dirs");
                    continue;
                }
                args.emplace_back("-I" + includeDir);
                appendBridgeDecision(report, "-I", true, "value='" + includeDir + "'");
            }
        }
        else
        {
            appendBridgeDecision(report, "-I", false, "empty stack_analyzer.include_dirs");
        }

        if (!config.stack_analyzer.defines.empty())
        {
            for (const auto& macroDef : config.stack_analyzer.defines)
            {
                if (macroDef.empty())
                {
                    appendBridgeDecision(report, "-D", false,
                                         "empty value in stack_analyzer.defines");
                    continue;
                }
                args.emplace_back("-D" + macroDef);
                appendBridgeDecision(report, "-D", true, "value='" + macroDef + "'");
            }
        }
        else
        {
            appendBridgeDecision(report, "-D", false, "empty stack_analyzer.defines");
        }

        if (!config.stack_analyzer.compile_args.empty())
        {
            for (const auto& compileArg : config.stack_analyzer.compile_args)
            {
                appendValueOption(args, report, "--compile-arg", compileArg,
                                  "empty value in stack_analyzer.compile_args");
            }
        }
        else
        {
            appendBridgeDecision(report, "--compile-arg", false,
                                 "empty stack_analyzer.compile_args");
        }

        appendFlagOption(args, report, "--timing", config.stack_analyzer.timing,
                         "coretrace timing enabled; hotspot summary is not exposed by the "
                         "analyzer library API yet (coretrace-stack-analyzer#93)",
                         "coretrace timing disabled");
        appendFlagOption(args, report, "--resource-summary-cache-memory-only",
                         config.stack_analyzer.resource_summary_cache_memory_only,
                         "stack_analyzer.resource_summary_cache_memory_only enabled",
                         "stack_analyzer.resource_summary_cache_memory_only disabled");
        if (config.stack_analyzer.resource_cross_tu.has_value())
        {
            const bool enabled = *config.stack_analyzer.resource_cross_tu;
            args.emplace_back(enabled ? "--resource-cross-tu" : "--no-resource-cross-tu");
            appendBridgeDecision(report, "resource_cross_tu", true,
                                 enabled ? "enabled" : "disabled");
        }
        else
        {
            appendBridgeDecision(report, "resource_cross_tu", false,
                                 "not set; analyzer default kept");
        }

        if (config.stack_analyzer.uninitialized_cross_tu.has_value())
        {
            const bool enabled = *config.stack_analyzer.uninitialized_cross_tu;
            args.emplace_back(enabled ? "--uninitialized-cross-tu" : "--no-uninitialized-cross-tu");
            appendBridgeDecision(report, "uninitialized_cross_tu", true,
                                 enabled ? "enabled" : "disabled");
        }
        else
        {
            appendBridgeDecision(report, "uninitialized_cross_tu", false,
                                 "not set; analyzer default kept");
        }
        if (config.stack_analyzer.stack_limit > 0)
        {
            args.emplace_back("--stack-limit");
            args.emplace_back(std::to_string(config.stack_analyzer.stack_limit));
            appendBridgeDecision(report, "--stack-limit", true,
                                 "value='" + std::to_string(config.stack_analyzer.stack_limit) +
                                     "'");
        }
        else
        {
            appendBridgeDecision(report, "--stack-limit", false,
                                 "value is 0; analyzer default kept");
        }
        if (config.stack_analyzer.smt_timeout_ms > 0)
        {
            args.emplace_back("--smt-timeout-ms");
            args.emplace_back(std::to_string(config.stack_analyzer.smt_timeout_ms));
            appendBridgeDecision(report, "--smt-timeout-ms", true,
                                 "value='" + std::to_string(config.stack_analyzer.smt_timeout_ms) +
                                     "'");
        }
        else
        {
            appendBridgeDecision(report, "--smt-timeout-ms", false,
                                 "value is 0; analyzer default kept");
        }
        if (config.stack_analyzer.smt_budget_nodes > 0)
        {
            args.emplace_back("--smt-budget-nodes");
            args.emplace_back(std::to_string(config.stack_analyzer.smt_budget_nodes));
            appendBridgeDecision(report, "--smt-budget-nodes", true,
                                 "value='" +
                                     std::to_string(config.stack_analyzer.smt_budget_nodes) + "'");
        }
        else
        {
            appendBridgeDecision(report, "--smt-budget-nodes", false,
                                 "value is 0; analyzer default kept");
        }
        if (!config.stack_analyzer.smt_rules.empty())
        {
            args.emplace_back("--smt-rules");
            args.emplace_back(joinCsv(config.stack_analyzer.smt_rules));
            appendBridgeDecision(report, "--smt-rules", true,
                                 "value='" + joinCsv(config.stack_analyzer.smt_rules) + "'");
        }
        else
        {
            appendBridgeDecision(report, "--smt-rules", false,
                                 "empty in coretrace config; analyzer default rules");
        }

        if (!config.stack_analyzer.extra_args.empty())
        {
            for (const auto& extraArg : config.stack_analyzer.extra_args)
            {
                if (extraArg.empty())
                {
                    continue;
                }
                args.emplace_back(extraArg);
                appendBridgeDecision(report, "extra-arg", true, "value='" + extraArg + "'");
            }
        }
        else
        {
            appendBridgeDecision(report, "extra-args", false,
                                 "no stack_analyzer.extra_args configured");
        }

        for (const auto& file : inputFiles)
        {
            args.push_back(file);
        }
        appendBridgeDecision(report, "input files", true,
                             "resolved_count=" + std::to_string(inputFiles.size()));
        appendBridgeDecision(
            report, "coretrace-only options", false,
            "--report-file/--output-file/--ipc* are handled by coretrace, not forwarded");

        return result;
    }

    [[nodiscard]] bool writeReportToFile(const std::string& reportPath, std::string_view content,
                                         std::string& errorMessage)
    {
        errorMessage.clear();
        if (reportPath.empty())
        {
            errorMessage = "report path is empty";
            return false;
        }

        try
        {
            const std::filesystem::path targetPath(reportPath);
            const auto parent = targetPath.parent_path();
            if (!parent.empty())
            {
                std::error_code mkdirError;
                std::filesystem::create_directories(parent, mkdirError);
                if (mkdirError)
                {
                    errorMessage = "failed to create report directory '" + parent.string() +
                                   "': " + mkdirError.message();
                    return false;
                }
            }

            std::ofstream out(targetPath, std::ios::binary | std::ios::trunc);
            if (!out.is_open())
            {
                errorMessage = "failed to open report file '" + targetPath.string() + "'";
                return false;
            }

            out.write(content.data(), static_cast<std::streamsize>(content.size()));
            if (!out.good())
            {
                errorMessage = "failed to write report file '" + targetPath.string() + "'";
                return false;
            }
        }
        catch (const std::exception& ex)
        {
            errorMessage = "failed to write report file '" + reportPath + "': " + ex.what();
            return false;
        }

        return true;
    }

    [[nodiscard]] ctrace::Severity toSeverity(ctrace::stack::DiagnosticSeverity severity)
    {
        switch (severity)
        {
        case ctrace::stack::DiagnosticSeverity::Info:
            return ctrace::Severity::Info;
        case ctrace::stack::DiagnosticSeverity::Error:
            return ctrace::Severity::Error;
        case ctrace::stack::DiagnosticSeverity::Warning:
            break;
        }
        return ctrace::Severity::Warning;
    }

    /// The per-file diagnostics, in report order: the same set `report.summary` counts.
    [[nodiscard]] std::vector<ctrace::Diagnostic>
    toDiagnostics(const ctrace::stack::app::AnalysisReport& report, const std::string& tool)
    {
        std::vector<ctrace::Diagnostic> diagnostics;
        for (const auto& file : report.files)
        {
            for (const auto& item : file.result.diagnostics)
            {
                diagnostics.push_back({tool, item.ruleId, item.filePath, item.line, item.column,
                                       toSeverity(item.severity), item.message, item.cweId});
            }
        }
        return diagnostics;
    }

    [[nodiscard]] std::string resolveStableReportPath(std::string_view reportPath)
    {
        if (reportPath.empty())
        {
            return {};
        }

        try
        {
            std::filesystem::path path(reportPath);
            if (path.is_relative())
            {
                path = std::filesystem::current_path() / path;
            }
            return path.lexically_normal().string();
        }
        catch (const std::exception&)
        {
            return std::string(reportPath);
        }
    }

} // namespace

namespace ctrace
{
    void StackAnalyzerToolImplementation::execute(const std::string& file,
                                                  const ctrace::ProgramConfig& config,
                                                  ToolOutput& output) const
    {
        executeBatch(std::vector<std::string>{file}, config, output);
    }

    void StackAnalyzerToolImplementation::executeBatch(const std::vector<std::string>& files,
                                                       const ctrace::ProgramConfig& config,
                                                       ToolOutput& output) const
    {
        const std::string stableReportPath = resolveStableReportPath(config.output.report_file);

        std::vector<std::string> inputFiles;
        inputFiles.reserve(files.size());
        for (const auto& file : files)
        {
            if (!file.empty())
            {
                inputFiles.push_back(file);
            }
        }

        if (inputFiles.empty())
        {
            output.error("Stack analyzer batch execution requested with no input files.");
            return;
        }

        if (inputFiles.size() == 1)
        {
            coretrace::log(coretrace::Level::Info, "Running CoreTrace Stack Analyzer on {}\n",
                           inputFiles.front());
        }
        else
        {
            coretrace::log(coretrace::Level::Info, "Running CoreTrace Stack Analyzer on {} files\n",
                           inputFiles.size());
        }

        const auto analyzerArgBuild = buildAnalyzerArgs(inputFiles, config);
        const std::vector<std::string>& analyzerArgs = analyzerArgBuild.args;

        if (config.output.verbose)
        {
            coretrace::log(coretrace::Level::Debug, coretrace::Module(kStackAnalyzerModule),
                           "CoreTrace -> stack_analyzer bridge report ({} entries)\n",
                           analyzerArgBuild.bridgeReport.size());
            for (const auto& line : analyzerArgBuild.bridgeReport)
            {
                coretrace::log(coretrace::Level::Debug, coretrace::Module(kStackAnalyzerModule),
                               "  {}\n", line);
            }
        }

        for (const auto& arg : analyzerArgs)
        {
            coretrace::log(coretrace::Level::Debug, "Analyzer argument: {}\n", arg);
        }

        auto parseResult = ctrace::stack::cli::parseArguments(analyzerArgs);
        if (parseResult.status == ctrace::stack::cli::ParseStatus::Error)
        {
            output.error(parseResult.error.empty() ? "Failed to parse stack analyzer arguments."
                                                   : parseResult.error);
            return;
        }
        if (parseResult.status == ctrace::stack::cli::ParseStatus::Help)
        {
            output.result("Stack analyzer help requested; analysis was not executed.");
            return;
        }

        // The analyzer runs in-process and hands its results back as data: no descriptor
        // capture, no text parsing. Its own status logs go through the shared logger.
        const ctrace::stack::cli::OutputFormat outputFormat = parseResult.parsed.outputFormat;
        ctrace::stack::app::ReportResult analysis;
        {
            const std::lock_guard<std::mutex> serializedRun(analyzerRunMutex());
            analysis = ctrace::stack::app::runAnalysis(std::move(parseResult.parsed));
        }
        if (!analysis.isOk())
        {
            output.error(analysis.error);
            return;
        }

        const ctrace::stack::app::AnalysisReport& report = *analysis.report;
        output.diagnostics(toDiagnostics(report, name()));

        const std::string rendered = ctrace::stack::app::renderReport(report, outputFormat);
        if (!rendered.empty())
        {
            if (config.runtime.ipc == "socket" && ipc)
            {
                ipc->write(rendered);
                output.record("stdout", rendered);
            }
            else
            {
                output.result(rendered);
            }
        }

        if (!stableReportPath.empty())
        {
            std::string writeError;
            if (!writeReportToFile(stableReportPath, rendered, writeError))
            {
                coretrace::log(coretrace::Level::Warn, coretrace::Module(kStackAnalyzerModule),
                               "Unable to persist stack analyzer report to '{}': {}\n",
                               stableReportPath, writeError);
            }
            else if (config.output.verbose)
            {
                coretrace::log(coretrace::Level::Debug, coretrace::Module(kStackAnalyzerModule),
                               "Stack analyzer report persisted to '{}' ({} bytes)\n",
                               stableReportPath, rendered.size());
            }
        }
    }

    std::string StackAnalyzerToolImplementation::name() const
    {
        return "ctrace_stack_analyzer";
    }

} // namespace ctrace
