#include "Process/Tools/AnalysisTools.hpp"
#include "app/AnalyzerApp.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <exception>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <utility>

#include <coretrace/logger.hpp>

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace
{
    constexpr std::string_view kStackAnalyzerModule = "stack_analyzer";

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

        args.reserve(inputFiles.size() + 32);
        report.reserve(32);

        args.emplace_back("--mode=ir");
        appendBridgeDecision(report, "--mode=ir", true, "forced by coretrace integration");
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

        appendFlagOption(args, report, "--format=json", config.global.hasSarifFormat,
                         "coretrace --sarif-format enabled", "coretrace --sarif-format disabled");
        appendFlagOption(args, report, "--verbose", config.global.verbose,
                         "coretrace --verbose enabled", "coretrace --verbose disabled");
        appendFlagOption(args, report, "--demangle", config.global.demangle,
                         "coretrace --demangle enabled", "coretrace --demangle disabled");
        appendFlagOption(args, report, "--quiet", config.global.quiet, "coretrace --quiet enabled",
                         "coretrace --quiet disabled");
        appendFlagOption(args, report, "--include-compdb-deps", config.global.include_compdb_deps,
                         "coretrace --include-compdb-deps enabled",
                         "coretrace --include-compdb-deps disabled");

        appendValueOption(args, report, "--analysis-profile", config.global.analysis_profile,
                          "empty in coretrace config; analyzer default kept");
        appendValueOption(args, report, "--compile-commands", config.global.compile_commands,
                          "empty in coretrace config; analyzer auto-discovery kept");
        appendValueOption(args, report, "--only-function", config.global.entry_points,
                          "empty in coretrace config; no entry-point filter");
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

        appendFlagOption(args, report, "--timing", config.global.timing, "coretrace timing enabled",
                         "coretrace timing disabled");
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
        else if (const auto parsedSummary = parseDiagnosticsSummaryFromText(capturedStderr);
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
