#include "Process/Tools/AnalysisTools.hpp"
#include "app/AnalyzerApp.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include <coretrace/logger.hpp>

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace
{
    constexpr std::string_view kStackAnalyzerModule = "stack_analyzer";
    using Json = nlohmann::json;

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
            config.global.stack_analyzer_mode.empty() ? "ir" : config.global.stack_analyzer_mode;
        args.emplace_back("--mode=" + analyzerMode);
        appendBridgeDecision(report, "--mode", true, "value='" + analyzerMode + "'");
        if (!config.global.config_file.empty())
        {
            appendBridgeDecision(report, "config source", true,
                                 "path='" + config.global.config_file +
                                     "' loaded by coretrace and mapped to analyzer options");
        }
        else
        {
            appendBridgeDecision(report, "config source", false,
                                 "no --config provided to coretrace");
        }

        appendValueOption(args, report, "--config", config.global.stack_analyzer_config,
                          "empty stack_analyzer.config; analyzer internal config disabled");
        appendFlagOption(args, report, "--print-effective-config",
                         config.global.stack_analyzer_print_effective_config,
                         "stack_analyzer.print_effective_config enabled",
                         "stack_analyzer.print_effective_config disabled");

        if (!config.global.stack_analyzer_output_format.empty())
        {
            args.emplace_back("--format=" + config.global.stack_analyzer_output_format);
            appendBridgeDecision(report, "--format", true,
                                 "value='" + config.global.stack_analyzer_output_format + "'");
        }
        else
        {
            appendFlagOption(args, report, "--format=json", config.global.hasSarifFormat,
                             "derived from coretrace --sarif-format",
                             "empty stack_analyzer.output_format and sarif disabled");
        }
        appendFlagOption(args, report, "--verbose", config.global.verbose,
                         "coretrace --verbose enabled", "coretrace --verbose disabled");
        appendFlagOption(args, report, "--demangle", config.global.demangle,
                         "coretrace --demangle enabled", "coretrace --demangle disabled");
        appendFlagOption(args, report, "--quiet", config.global.quiet, "coretrace --quiet enabled",
                         "coretrace --quiet disabled");
        appendFlagOption(args, report, "--include-compdb-deps", config.global.include_compdb_deps,
                         "coretrace --include-compdb-deps enabled",
                         "coretrace --include-compdb-deps disabled");
        appendFlagOption(args, report, "--compdb-fast", config.global.stack_analyzer_compdb_fast,
                         "stack_analyzer.compdb_fast enabled",
                         "stack_analyzer.compdb_fast disabled");
        appendFlagOption(args, report, "--STL", config.global.stack_analyzer_include_stl,
                         "stack_analyzer.include_stl enabled",
                         "stack_analyzer.include_stl disabled");
        appendFlagOption(args, report, "--warnings-only", config.global.stack_analyzer_warnings_only,
                         "stack_analyzer.warnings_only enabled",
                         "stack_analyzer.warnings_only disabled");
        appendFlagOption(args, report, "--dump-filter", config.global.stack_analyzer_dump_filter,
                         "stack_analyzer.dump_filter enabled",
                         "stack_analyzer.dump_filter disabled");

        appendValueOption(args, report, "--analysis-profile", config.global.analysis_profile,
                          "empty in coretrace config; analyzer default kept");
        appendValueOption(args, report, "--compile-commands", config.global.compile_commands,
                          "empty in coretrace config; analyzer auto-discovery kept");
        if (!config.global.stack_analyzer_jobs.empty())
        {
            appendValueOption(args, report, "--jobs", config.global.stack_analyzer_jobs,
                              "empty stack_analyzer.jobs");
        }
        else
        {
            appendBridgeDecision(report, "--jobs", false,
                                 "empty stack_analyzer.jobs; analyzer default kept");
        }

        appendValueOption(args, report, "--resource-summary-cache-dir",
                          config.global.stack_analyzer_resource_summary_cache_dir,
                          "empty stack_analyzer.resource_summary_cache_dir");
        appendValueOption(args, report, "--compile-ir-cache-dir",
                          config.global.stack_analyzer_compile_ir_cache_dir,
                          "empty stack_analyzer.compile_ir_cache_dir");
        appendValueOption(args, report, "--compile-ir-format",
                          config.global.stack_analyzer_compile_ir_format,
                          "empty stack_analyzer.compile_ir_format");
        appendValueOption(args, report, "--dump-ir", config.global.stack_analyzer_dump_ir,
                          "empty stack_analyzer.dump_ir");
        appendValueOption(args, report, "--base-dir", config.global.stack_analyzer_base_dir,
                          "empty stack_analyzer.base_dir");
        appendValueOption(args, report, "--resource-model", config.global.resource_model,
                          "empty in coretrace config; analyzer default model");
        appendValueOption(args, report, "--escape-model", config.global.escape_model,
                          "empty in coretrace config; analyzer default model");
        appendValueOption(args, report, "--buffer-model", config.global.buffer_model,
                          "empty in coretrace config; analyzer default model");
        appendValueOption(args, report, "--smt", config.global.smt,
                          "empty in coretrace config; analyzer default SMT");
        appendValueOption(args, report, "--smt-backend", config.global.smt_backend,
                          "empty in coretrace config; analyzer default backend");
        appendValueOption(args, report, "--smt-secondary-backend",
                          config.global.smt_secondary_backend,
                          "empty in coretrace config; analyzer default secondary backend");
        appendValueOption(args, report, "--smt-mode", config.global.smt_mode,
                          "empty in coretrace config; analyzer default mode");

        if (!config.global.stack_analyzer_only_files.empty())
        {
            for (const auto& filter : config.global.stack_analyzer_only_files)
            {
                appendValueOption(args, report, "--only-file", filter,
                                  "empty value in stack_analyzer.only_files");
            }
        }
        else
        {
            appendBridgeDecision(report, "--only-file", false,
                                 "empty stack_analyzer.only_files");
        }

        if (!config.global.stack_analyzer_only_dirs.empty())
        {
            for (const auto& filter : config.global.stack_analyzer_only_dirs)
            {
                appendValueOption(args, report, "--only-dir", filter,
                                  "empty value in stack_analyzer.only_dirs");
            }
        }
        else
        {
            appendBridgeDecision(report, "--only-dir", false,
                                 "empty stack_analyzer.only_dirs");
        }

        if (!config.global.stack_analyzer_exclude_dirs.empty())
        {
            for (const auto& filter : config.global.stack_analyzer_exclude_dirs)
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

        if (!config.global.stack_analyzer_only_functions.empty())
        {
            args.emplace_back("--only-func");
            args.emplace_back(joinCsv(config.global.stack_analyzer_only_functions));
            appendBridgeDecision(report, "--only-func", true,
                                 "value='" +
                                     joinCsv(config.global.stack_analyzer_only_functions) + "'");
        }
        else
        {
            appendValueOption(args, report, "--only-function", config.global.entry_points,
                              "empty in coretrace config; no entry-point filter");
        }

        if (!config.global.stack_analyzer_include_dirs.empty())
        {
            for (const auto& includeDir : config.global.stack_analyzer_include_dirs)
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
            appendBridgeDecision(report, "-I", false,
                                 "empty stack_analyzer.include_dirs");
        }

        if (!config.global.stack_analyzer_defines.empty())
        {
            for (const auto& macroDef : config.global.stack_analyzer_defines)
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

        if (!config.global.stack_analyzer_compile_args.empty())
        {
            for (const auto& compileArg : config.global.stack_analyzer_compile_args)
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

        appendFlagOption(args, report, "--timing", config.global.timing, "coretrace timing enabled",
                         "coretrace timing disabled");
        appendFlagOption(args, report, "--resource-summary-cache-memory-only",
                         config.global.stack_analyzer_resource_summary_cache_memory_only,
                         "stack_analyzer.resource_summary_cache_memory_only enabled",
                         "stack_analyzer.resource_summary_cache_memory_only disabled");
        if (config.global.stack_analyzer_resource_cross_tu.has_value())
        {
            const bool enabled = *config.global.stack_analyzer_resource_cross_tu;
            args.emplace_back(enabled ? "--resource-cross-tu" : "--no-resource-cross-tu");
            appendBridgeDecision(report, "resource_cross_tu", true,
                                 enabled ? "enabled" : "disabled");
        }
        else
        {
            appendBridgeDecision(report, "resource_cross_tu", false,
                                 "not set; analyzer default kept");
        }

        if (config.global.stack_analyzer_uninitialized_cross_tu.has_value())
        {
            const bool enabled = *config.global.stack_analyzer_uninitialized_cross_tu;
            args.emplace_back(enabled ? "--uninitialized-cross-tu"
                                      : "--no-uninitialized-cross-tu");
            appendBridgeDecision(report, "uninitialized_cross_tu", true,
                                 enabled ? "enabled" : "disabled");
        }
        else
        {
            appendBridgeDecision(report, "uninitialized_cross_tu", false,
                                 "not set; analyzer default kept");
        }
        if (config.global.stack_limit > 0)
        {
            args.emplace_back("--stack-limit");
            args.emplace_back(std::to_string(config.global.stack_limit));
            appendBridgeDecision(report, "--stack-limit", true,
                                 "value='" + std::to_string(config.global.stack_limit) + "'");
        }
        else
        {
            appendBridgeDecision(report, "--stack-limit", false,
                                 "value is 0; analyzer default kept");
        }
        if (config.global.smt_timeout_ms > 0)
        {
            args.emplace_back("--smt-timeout-ms");
            args.emplace_back(std::to_string(config.global.smt_timeout_ms));
            appendBridgeDecision(report, "--smt-timeout-ms", true,
                                 "value='" + std::to_string(config.global.smt_timeout_ms) + "'");
        }
        else
        {
            appendBridgeDecision(report, "--smt-timeout-ms", false,
                                 "value is 0; analyzer default kept");
        }
        if (config.global.smt_budget_nodes > 0)
        {
            args.emplace_back("--smt-budget-nodes");
            args.emplace_back(std::to_string(config.global.smt_budget_nodes));
            appendBridgeDecision(report, "--smt-budget-nodes", true,
                                 "value='" + std::to_string(config.global.smt_budget_nodes) + "'");
        }
        else
        {
            appendBridgeDecision(report, "--smt-budget-nodes", false,
                                 "value is 0; analyzer default kept");
        }
        if (!config.global.smt_rules.empty())
        {
            args.emplace_back("--smt-rules");
            args.emplace_back(joinCsv(config.global.smt_rules));
            appendBridgeDecision(report, "--smt-rules", true,
                                 "value='" + joinCsv(config.global.smt_rules) + "'");
        }
        else
        {
            appendBridgeDecision(report, "--smt-rules", false,
                                 "empty in coretrace config; analyzer default rules");
        }

        if (!config.global.stack_analyzer_extra_args.empty())
        {
            for (const auto& extraArg : config.global.stack_analyzer_extra_args)
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

    [[nodiscard]] std::optional<ctrace::DiagnosticSummary>
    parseDiagnosticsSummaryFromText(std::string_view text)
    {
        static const std::regex kTotalSummaryPattern(
            R"(Total diagnostics summary:\s*info=(\d+),\s*warning=(\d+),\s*error=(\d+))");
        static const std::regex kSummaryPattern(
            R"(Diagnostics summary:\s*info=(\d+),\s*warning=(\d+),\s*error=(\d+))");

        std::string captured(text);
        auto parseSummary = [](const std::smatch& match) -> std::optional<ctrace::DiagnosticSummary>
        {
            ctrace::DiagnosticSummary current{};
            try
            {
                current.info = static_cast<std::size_t>(std::stoull(match[1].str()));
                current.warning = static_cast<std::size_t>(std::stoull(match[2].str()));
                current.error = static_cast<std::size_t>(std::stoull(match[3].str()));
            }
            catch (const std::exception&)
            {
                return std::nullopt;
            }
            return current;
        };

        std::optional<ctrace::DiagnosticSummary> totalSummary;
        for (std::sregex_iterator it(captured.begin(), captured.end(), kTotalSummaryPattern), end;
             it != end; ++it)
        {
            if (const auto parsed = parseSummary(*it); parsed.has_value())
            {
                totalSummary = parsed;
            }
        }
        if (totalSummary.has_value())
        {
            return totalSummary;
        }

        std::optional<ctrace::DiagnosticSummary> summary;
        for (std::sregex_iterator it(captured.begin(), captured.end(), kSummaryPattern), end;
             it != end; ++it)
        {
            const auto parsed = parseSummary(*it);
            if (!parsed.has_value())
            {
                continue;
            }
            if (!summary.has_value())
            {
                summary = *parsed;
                continue;
            }

            summary->info += parsed->info;
            summary->warning += parsed->warning;
            summary->error += parsed->error;
        }

        return summary;
    }

    [[nodiscard]] std::string toLowerAscii(std::string_view input)
    {
        std::string lowered;
        lowered.reserve(input.size());
        for (const char ch : input)
        {
            lowered.push_back(static_cast<char>(
                std::tolower(static_cast<unsigned char>(ch))));
        }
        return lowered;
    }

    void accumulateSeverity(ctrace::DiagnosticSummary& summary, std::string_view severity)
    {
        const std::string lowered = toLowerAscii(severity);
        if (lowered == "error")
        {
            ++summary.error;
            return;
        }
        if (lowered == "warning" || lowered == "warn")
        {
            ++summary.warning;
            return;
        }
        if (lowered == "info" || lowered == "information" || lowered == "note")
        {
            ++summary.info;
        }
    }

    [[nodiscard]] std::optional<ctrace::DiagnosticSummary>
    parseDiagnosticsSummaryFromStructuredOutput(std::string_view text)
    {
        Json root = Json::parse(text, nullptr, false);
        if (root.is_discarded() || !root.is_object())
        {
            return std::nullopt;
        }

        if (const auto diagnosticsIt = root.find("diagnostics");
            diagnosticsIt != root.end() && diagnosticsIt->is_array())
        {
            ctrace::DiagnosticSummary summary{};
            for (const auto& diagnostic : *diagnosticsIt)
            {
                if (!diagnostic.is_object())
                {
                    continue;
                }
                const auto severityIt = diagnostic.find("severity");
                if (severityIt == diagnostic.end() || !severityIt->is_string())
                {
                    continue;
                }
                accumulateSeverity(summary, severityIt->get_ref<const std::string&>());
            }
            return summary;
        }

        if (const auto runsIt = root.find("runs"); runsIt != root.end() && runsIt->is_array())
        {
            ctrace::DiagnosticSummary summary{};
            bool hasSarifResults = false;
            for (const auto& run : *runsIt)
            {
                if (!run.is_object())
                {
                    continue;
                }
                const auto resultsIt = run.find("results");
                if (resultsIt == run.end() || !resultsIt->is_array())
                {
                    continue;
                }
                for (const auto& result : *resultsIt)
                {
                    if (!result.is_object())
                    {
                        continue;
                    }
                    hasSarifResults = true;
                    const auto levelIt = result.find("level");
                    if (levelIt != result.end() && levelIt->is_string())
                    {
                        accumulateSeverity(summary, levelIt->get_ref<const std::string&>());
                    }
                    else
                    {
                        // SARIF defaults missing level to "warning".
                        ++summary.warning;
                    }
                }
            }
            if (hasSarifResults)
            {
                return summary;
            }
        }

        return std::nullopt;
    }

    void captureToolOutputOnly(const std::string& stream, const std::string& message)
    {
        const auto* ctx = ctrace::Thread::Output::capture_context;
        if (!ctx || !ctx->buffer || message.empty())
        {
            return;
        }
        ctx->buffer->append(ctx->tool, stream, message);
    }

    void logMultiline(coretrace::Level level, std::string_view module, std::string_view message,
                      std::string_view prefix = {})
    {
        std::size_t start = 0;
        while (start <= message.size())
        {
            const auto end = message.find('\n', start);
            std::string_view line = (end == std::string_view::npos)
                                        ? message.substr(start)
                                        : message.substr(start, end - start);

            if (!line.empty() && line.back() == '\r')
            {
                line.remove_suffix(1);
            }

            if (!line.empty())
            {
                if (prefix.empty())
                {
                    coretrace::log(level, coretrace::Module(module), "{}\n", line);
                }
                else
                {
                    coretrace::log(level, coretrace::Module(module), "{}{}\n", prefix, line);
                }
            }

            if (end == std::string_view::npos)
            {
                break;
            }
            start = end + 1;
        }
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

#if !defined(_WIN32)
    struct CapturedStreams
    {
        std::string stdoutText;
        std::string stderrText;
    };

    [[nodiscard]] std::string readFdFully(int fd)
    {
        if (fd < 0)
        {
            return {};
        }

        if (lseek(fd, 0, SEEK_SET) == -1)
        {
            return {};
        }

        std::string content;
        std::array<char, 4096> buffer{};
        for (;;)
        {
            const auto bytes = read(fd, buffer.data(), buffer.size());
            if (bytes <= 0)
            {
                break;
            }
            content.append(buffer.data(), static_cast<std::size_t>(bytes));
        }
        return content;
    }

    class ScopedFdCapture
    {
      public:
        ScopedFdCapture()
        {
            enabled_ = initialize();
        }

        ~ScopedFdCapture()
        {
            if (!released_)
            {
                (void)release();
            }
        }

        [[nodiscard]] bool enabled() const
        {
            return enabled_;
        }

        CapturedStreams release()
        {
            if (released_)
            {
                return {};
            }

            flushStreams();

            CapturedStreams captured{
                readFdFully(capturedStdoutFd_),
                readFdFully(capturedStderrFd_),
            };

            restoreDescriptors();
            closeTempFiles();
            released_ = true;
            return captured;
        }

      private:
        static int createTempFile(char* templ)
        {
            const int fd = mkstemp(templ);
            if (fd >= 0)
            {
                unlink(templ);
            }
            return fd;
        }

        static void flushStreams()
        {
            std::fflush(stdout);
            std::fflush(stderr);
            std::cout.flush();
            std::cerr.flush();
            llvm::outs().flush();
            llvm::errs().flush();
        }

        bool initialize()
        {
            flushStreams();

            originalStdoutFd_ = dup(STDOUT_FILENO);
            originalStderrFd_ = dup(STDERR_FILENO);
            if (originalStdoutFd_ < 0 || originalStderrFd_ < 0)
            {
                return false;
            }

            char stdoutTemplate[] = "/tmp/ctrace-stack-stdout-XXXXXX";
            char stderrTemplate[] = "/tmp/ctrace-stack-stderr-XXXXXX";

            capturedStdoutFd_ = createTempFile(stdoutTemplate);
            capturedStderrFd_ = createTempFile(stderrTemplate);
            if (capturedStdoutFd_ < 0 || capturedStderrFd_ < 0)
            {
                return false;
            }

            if (dup2(capturedStdoutFd_, STDOUT_FILENO) == -1)
            {
                return false;
            }
            if (dup2(capturedStderrFd_, STDERR_FILENO) == -1)
            {
                return false;
            }

            return true;
        }

        void restoreDescriptors()
        {
            if (!enabled_)
            {
                return;
            }

            if (originalStdoutFd_ >= 0)
            {
                (void)dup2(originalStdoutFd_, STDOUT_FILENO);
            }
            if (originalStderrFd_ >= 0)
            {
                (void)dup2(originalStderrFd_, STDERR_FILENO);
            }

            if (originalStdoutFd_ >= 0)
            {
                close(originalStdoutFd_);
                originalStdoutFd_ = -1;
            }
            if (originalStderrFd_ >= 0)
            {
                close(originalStderrFd_);
                originalStderrFd_ = -1;
            }
        }

        void closeTempFiles()
        {
            if (capturedStdoutFd_ >= 0)
            {
                close(capturedStdoutFd_);
                capturedStdoutFd_ = -1;
            }
            if (capturedStderrFd_ >= 0)
            {
                close(capturedStderrFd_);
                capturedStderrFd_ = -1;
            }
        }

        bool enabled_ = false;
        bool released_ = false;
        int originalStdoutFd_ = -1;
        int originalStderrFd_ = -1;
        int capturedStdoutFd_ = -1;
        int capturedStderrFd_ = -1;
    };
#endif
} // namespace

namespace ctrace
{
    void StackAnalyzerToolImplementation::execute(const std::string& file,
                                                  ctrace::ProgramConfig config) const
    {
        executeBatch(std::vector<std::string>{file}, std::move(config));
    }

    void StackAnalyzerToolImplementation::executeBatch(const std::vector<std::string>& files,
                                                       ctrace::ProgramConfig config) const
    {
        m_lastDiagnosticsSummary = {};
        const std::string stableReportPath = resolveStableReportPath(config.global.report_file);

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
            ctrace::Thread::Output::tool_err(
                "Stack analyzer batch execution requested with no input files.");
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

        if (config.global.verbose)
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
            if (parseResult.error.empty())
            {
                ctrace::Thread::Output::tool_err("Failed to parse stack analyzer arguments.");
            }
            else
            {
                ctrace::Thread::Output::tool_err(parseResult.error);
            }
            return;
        }
        if (parseResult.status == ctrace::stack::cli::ParseStatus::Help)
        {
            ctrace::Thread::Output::tool_out(
                "Stack analyzer help requested; analysis was not executed.");
            return;
        }

#if !defined(_WIN32)
        ScopedFdCapture processOutputCapture;
#endif

        const ctrace::stack::app::RunResult runResult =
            ctrace::stack::app::runAnalyzerApp(std::move(parseResult.parsed));

        std::string capturedStdout;
        std::string capturedStderr;
#if !defined(_WIN32)
        if (processOutputCapture.enabled())
        {
            const auto captured = processOutputCapture.release();
            if (!captured.stdoutText.empty())
            {
                capturedStdout = captured.stdoutText;
                captureToolOutputOnly("stdout", captured.stdoutText);
                if (config.global.ipc == "standardIO")
                {
                    ctrace::Thread::Output::tool_out(captured.stdoutText);
                }
                else if (config.global.ipc == "socket" && ipc)
                {
                    ipc->write(captured.stdoutText);
                }
            }
            if (!captured.stderrText.empty())
            {
                capturedStderr = captured.stderrText;
                captureToolOutputOnly("stderr", captured.stderrText);
                logMultiline(coretrace::Level::Warn, kStackAnalyzerModule, captured.stderrText);
            }
        }
#endif

        if (const auto parsedSummary = parseDiagnosticsSummaryFromText(capturedStdout);
            parsedSummary.has_value())
        {
            m_lastDiagnosticsSummary = *parsedSummary;
        }
        else if (const auto parsedSummary = parseDiagnosticsSummaryFromStructuredOutput(capturedStdout);
                 parsedSummary.has_value())
        {
            m_lastDiagnosticsSummary = *parsedSummary;
        }
        else if (const auto parsedSummary = parseDiagnosticsSummaryFromText(capturedStderr);
                 parsedSummary.has_value())
        {
            m_lastDiagnosticsSummary = *parsedSummary;
        }
        else if (const auto parsedSummary = parseDiagnosticsSummaryFromStructuredOutput(capturedStderr);
                 parsedSummary.has_value())
        {
            m_lastDiagnosticsSummary = *parsedSummary;
        }

        if (!runResult.isOk())
        {
            ctrace::Thread::Output::tool_err(runResult.error);
            return;
        }

        if (runResult.exitCode != 0)
        {
            ctrace::Thread::Output::tool_err("Stack analyzer exited with code " +
                                             std::to_string(runResult.exitCode));
            return;
        }

        if (!stableReportPath.empty())
        {
            std::string writeError;
            if (!writeReportToFile(stableReportPath, capturedStdout, writeError))
            {
                coretrace::log(coretrace::Level::Warn, coretrace::Module(kStackAnalyzerModule),
                               "Unable to persist stack analyzer report to '{}': {}\n",
                               stableReportPath, writeError);
            }
            else if (config.global.verbose)
            {
                coretrace::log(coretrace::Level::Debug, coretrace::Module(kStackAnalyzerModule),
                               "Stack analyzer report persisted to '{}' ({} bytes)\n",
                               stableReportPath, capturedStdout.size());
            }
        }
    }

    std::string StackAnalyzerToolImplementation::name() const
    {
        return "ctrace_stack_analyzer";
    }

    DiagnosticSummary StackAnalyzerToolImplementation::lastDiagnosticsSummary() const
    {
        return m_lastDiagnosticsSummary;
    }

} // namespace ctrace
