// SPDX-License-Identifier: Apache-2.0
#ifndef ANALYSIS_TOOLS_HPP
#define ANALYSIS_TOOLS_HPP

#include "AnalysisToolsBase.hpp"
#include "ctrace_tools/languageType.hpp"
#include "ctrace_tools/mangle.hpp"
#include "RunDirectory.hpp"
#include "../ProcessFactory.hpp"
#include "ToolOutput.hpp"

#include <coretrace/logger.hpp>
#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

class EntryPoint
{
  public:
    EntryPoint(const std::string& entryPointName, const std::vector<std::string>& paramTypes)
        : name(entryPointName), paramTypes(paramTypes)
    {
        m_isMangled = ctrace_tools::mangle::isMangled(name);

        // C++ never mangles main: its symbol is the plain name.
        if (m_isMangled || name == "main")
        {
            mangledName = name;
        }
        else
        {
            mangledName = ctrace_tools::mangle::mangleFunction("", name, paramTypes);
        }
    }
    ~EntryPoint() = default;
    [[nodiscard]] std::string_view getEntryPointNameCMode() const noexcept
    {
        if (name.empty())
        {
            return "unnamed";
        }
        return name;
    }
    [[nodiscard]] std::string_view getEntryPointNameCCMode() const noexcept
    {
        if (mangledName.empty())
        {
            return "unnamed";
        }
        return getMangledName();
    }
    [[nodiscard]] std::string_view getMangledName() const noexcept
    {
        if (mangledName.empty())
        {
            return "unnamed";
        }
        return mangledName;
    }

  protected:
    std::string name;
    std::string mangledName;
    std::vector<std::string> paramTypes;

  private:
    bool m_isMangled = false;
};

namespace ctrace
{
    /// The command to run for an external tool: the configured path when there is one,
    /// otherwise the tool's own name, which the process layer resolves through PATH.
    [[nodiscard]] inline std::string toolCommand(const ProgramConfig& config,
                                                 const IAnalysisTool& tool)
    {
        const std::string name = tool.name();
        const auto configured = config.tools.paths.find(name);
        if (configured != config.tools.paths.end() && !configured->second.empty())
        {
            return configured->second;
        }
        return name;
    }

    /// Appends `tools.<name>.args`, what the user adds to a tool's command line, verbatim.
    inline void appendToolArguments(std::vector<std::string>& args, const ProgramConfig& config,
                                    const std::string& tool)
    {
        if (const auto configured = config.tools.args.find(tool);
            configured != config.tools.args.end())
        {
            args.insert(args.end(), configured->second.begin(), configured->second.end());
        }
    }

    /// Appends the build context every C/C++ tool receives: `-I<dir>` for each
    /// `stack_analyzer.include_dirs` entry, then `-D<macro>` for each `stack_analyzer.defines`
    /// entry. Empty entries are skipped: a bare `-I` would take the next argument as its value.
    inline void appendBuildContext(std::vector<std::string>& args, const ProgramConfig& config)
    {
        for (const std::string& dir : config.stack_analyzer.include_dirs)
        {
            if (!dir.empty())
            {
                args.push_back("-I" + dir);
            }
        }
        for (const std::string& macro : config.stack_analyzer.defines)
        {
            if (!macro.empty())
            {
                args.push_back("-D" + macro);
            }
        }
    }

    /// How long one run of `tool` may take: `tools.<name>.timeout_s`, else the default. Zero
    /// means no limit.
    [[nodiscard]] inline std::chrono::seconds toolTimeout(const ProgramConfig& config,
                                                          const std::string& tool)
    {
        const auto configured = config.tools.timeouts_s.find(tool);
        return std::chrono::seconds(configured != config.tools.timeouts_s.end()
                                        ? configured->second
                                        : ToolsConfig::kDefaultTimeoutSeconds);
    }

    /// Runs an external tool to completion. A tool that cannot be started is reported on the
    /// sink and yields nothing. An exit code outside `completedCodes` (by default only 0) is
    /// reported too, but the output is still returned: partial findings are not lost because
    /// the tool ended badly. A tool still running after `timeout` (by default its toolTimeout)
    /// is stopped and reported the same way.
    [[nodiscard]] inline std::optional<ProcessResult>
    runExternalTool(const ProgramConfig& config, const IAnalysisTool& tool,
                    const std::vector<std::string>& args, ToolOutput& output,
                    std::initializer_list<int> completedCodes = {0},
                    std::optional<std::chrono::milliseconds> timeout = std::nullopt)
    {
        try
        {
            auto process =
                ProcessFactory::createProcess(toolCommand(config, tool), args,
                                              timeout.value_or(toolTimeout(config, tool.name())));
            const ProcessResult run = process->execute();
            if (!run.completedWith(completedCodes))
            {
                output.error(run.describeFailure(tool.name()));
            }
            return run;
        }
        catch (const std::exception& e)
        {
            output.error("Error: " + std::string(e.what()));
            return std::nullopt;
        }
    }

    // Static analysis tools
    class IkosToolImplementation : public AnalysisToolBase
    {
      public:
        /// ikos has no SARIF writer: a SARIF request maps to its structured JSON output,
        /// which the bridge can convert, and plain runs keep the human-readable text.
        /// `database` is where the run writes its results database, ikos's own work file.
        [[nodiscard]] static std::vector<std::string>
        buildArguments(const ctrace::ProgramConfig& config, const std::string& file,
                       const std::filesystem::path& database)
        {
            std::vector<std::string> args;
            args.push_back(config.output.sarif_format ? "--format=json" : "--format=text");
            args.push_back("--output-db=" + database.string());
            args.push_back("-a=upa,dfa,pcmp,poa,nullity,fca");
            args.push_back("-d=congruence");
            args.push_back("--partitioning=return");

            const bool isC = ctrace_tools::detectLanguage(file) == ctrace_defs::LanguageType::C;
            std::vector<std::string> entry_points;
            for (const std::string& name : config.files.entry_points)
            {
                const EntryPoint entryPoint(name, {"void"}); // TODO parse function parameters
                entry_points.emplace_back(isC ? entryPoint.getEntryPointNameCMode()
                                              : entryPoint.getEntryPointNameCCMode());
            }
            // Without configured entry points, ikos keeps its own default: main.
            if (!entry_points.empty())
            {
                args.push_back("--entry-points=" +
                               ctrace_tools::strings::joinByComma(entry_points));
            }
            appendToolArguments(args, config, "ikos");
            args.push_back(file);
            return args;
        }

        /// ikos output is not interpreted yet: it is shown as is and the tool is reported as
        /// uninterpreted rather than counted as zero findings.
        void execute(const std::string& file, const ctrace::ProgramConfig& config,
                     ToolOutput& output) const override
        {
            coretrace::log(coretrace::Level::Info, "Running ikos on {}\n", file);
            const RunDirectory runDirectory("ctrace-ikos");
            const auto run = runExternalTool(
                config, *this, buildArguments(config, file, runDirectory.path() / "output.db"),
                output);
            if (!run)
            {
                return;
            }
            if (config.output.sarif_format)
            {
                // Not part of the merged document: kept for the API, out of stdout.
                output.record("stdout", run->output);
                coretrace::log(coretrace::Level::Warn, coretrace::Module(name()),
                               "output is not interpreted; its findings are not in the SARIF "
                               "document\n");
                return;
            }
            output.result(run->output);
        }
        std::string name() const override
        {
            return "ikos";
        }
    };

    class StackAnalyzerToolImplementation : public AnalysisToolBase
    {
      public:
        void execute(const std::string& file, const ctrace::ProgramConfig& config,
                     ToolOutput& output) const override;
        [[nodiscard]] bool supportsBatchExecution() const override
        {
            return true;
        }
        void executeBatch(const std::vector<std::string>& files,
                          const ctrace::ProgramConfig& config, ToolOutput& output) const override;
        std::string name() const override;
    };

    /// coretrace-concurrency-analyzer (github.com/CoreTrace/coretrace-concurrency-analyzer),
    /// linked into ctrace like the stack analyzer: data races on shared globals, lock-order
    /// deadlocks, missing joins, condition waits without a predicate, thread arguments escaping
    /// their frame, unsafe signal handlers, weak publication. With a compilation database the
    /// inputs are analyzed as one program, each unit built as the database says, so that a
    /// thread started in one unit is related to its body in another; otherwise each input is
    /// analyzed on its own, built with `stack_analyzer.include_dirs` and `.defines`. An input
    /// that does not compile, or that the database does not list, makes the run incomplete.
    class ConcurrencyAnalyzerToolImplementation : public AnalysisToolBase
    {
      public:
        void execute(const std::string& file, const ProgramConfig& config,
                     ToolOutput& output) const override
        {
            executeBatch({file}, config, output);
        }
        [[nodiscard]] bool supportsBatchExecution() const override
        {
            return true;
        }
        void executeBatch(const std::vector<std::string>& files, const ProgramConfig& config,
                          ToolOutput& output) const override;
        std::string name() const override;
    };

    class FlawfinderToolImplementation : public AnalysisToolBase
    {
      public:
        [[nodiscard]] static std::vector<std::string>
        buildArguments(const ctrace::ProgramConfig& config, const std::string& file);
        /// Reads flawfinder's SARIF output; nothing when the output is not SARIF.
        [[nodiscard]] static std::optional<std::vector<Diagnostic>>
        parseDiagnostics(const std::string& output);
        void execute(const std::string& file, const ctrace::ProgramConfig& config,
                     ToolOutput& output) const override;
        std::string name() const override;

      private:
        void emit(const ctrace::ProgramConfig& config, ToolOutput& output, const std::string& text,
                  bool toConsole) const;
    };

    /// coretrace-python-analyzer (github.com/CoreTrace/coretrace-python-analyzer). It analyzes a
    /// whole project, so that a flaw crossing modules is found: it runs once per project root of
    /// the Python inputs, and only the findings of the inputs and of the project itself (its
    /// dependency manifests) are kept. Its exit code carries a
    /// verdict: 0 clean, 1 findings, 2 error.
    class PythonAnalyzerToolImplementation : public AnalysisToolBase
    {
      public:
        [[nodiscard]] static std::vector<std::string>
        buildArguments(const ctrace::ProgramConfig& config, const std::string& projectRoot);
        /// The directory the analyzer names modules from (app/helpers.py is app.helpers): the
        /// nearest directory above `file` that holds a project marker (pyproject.toml,
        /// setup.py, setup.cfg, .git), else the file's own directory.
        [[nodiscard]] static std::filesystem::path projectRoot(const std::string& file);
        /// Reads the analyzer's SARIF for the project at `root`: the findings located in one of
        /// `inputs`, spelled as the input was, and those about the project itself (outside any
        /// Python file, such as a vulnerable pin in requirements.txt), spelled relative to the
        /// working directory when below it. Findings in other Python files are dropped.
        /// Nothing when the output is not SARIF.
        [[nodiscard]] static std::optional<std::vector<Diagnostic>>
        parseDiagnostics(const std::string& output, const std::filesystem::path& root,
                         const std::vector<std::string>& inputs);
        [[nodiscard]] bool analyzes(ctrace_defs::LanguageType language) const override
        {
            return language == ctrace_defs::LanguageType::Python;
        }
        [[nodiscard]] bool supportsBatchExecution() const override
        {
            return true;
        }
        void execute(const std::string& file, const ctrace::ProgramConfig& config,
                     ToolOutput& output) const override
        {
            executeBatch({file}, config, output);
        }
        void executeBatch(const std::vector<std::string>& files,
                          const ctrace::ProgramConfig& config, ToolOutput& output) const override;
        std::string name() const override;
    };

    /// coretrace-runtime-analyzer (github.com/CoreTrace/coretrace-runtime-analyzer): builds one
    /// program with CoreTrace instrumentation, runs it, and reports the memory errors it hit
    /// as SARIF. Each C or C++ input is a program. The tool runs as its own process, bundled
    /// next to ctrace: a crash or hang of the tool or of the program does not reach ctrace.
    /// That process is not a sandbox; the program runs with ctrace's privileges.
    class RuntimeAnalyzerToolImplementation : public AnalysisToolBase
    {
      public:
        /// `program` is where the instrumented binary is written, in ctrace's run directory.
        [[nodiscard]] static std::vector<std::string>
        buildArguments(const ProgramConfig& config, const std::string& file,
                       const std::filesystem::path& program);
        void execute(const std::string& file, const ProgramConfig& config,
                     ToolOutput& output) const override;
        /// One program at a time: programs built and run together would compete for the
        /// machine and against their own timeouts.
        [[nodiscard]] std::size_t maxConcurrentRuns() const override
        {
            return 1;
        }
        std::string name() const override;
    };

    class TscancodeToolImplementation : public AnalysisToolBase
    {
      public:
        [[nodiscard]] static std::vector<std::string> buildArguments(const ProgramConfig& config,
                                                                     const std::string& file);
        void execute(const std::string& file, const ProgramConfig& config,
                     ToolOutput& output) const override;
        std::string name() const override;

        /// Reads tscancode's `[file:line]: (severity) message` lines.
        [[nodiscard]] static std::vector<Diagnostic> parseDiagnostics(const std::string& output);
    };

    class CppCheckToolImplementation : public AnalysisToolBase
    {
      public:
        /// The text output contract: one line per finding, on every cppcheck version. The CWE
        /// (0 when the check has none) lets a finding be recognized when another tool reports
        /// the same weakness.
        static constexpr const char* kOutputTemplate =
            "{file}:{line}:{column}: {severity}: {message} [{id}] [CWE-{cwe}]";

        [[nodiscard]] static std::vector<std::string>
        buildArguments(const ctrace::ProgramConfig& config, const std::string& file);
        /// Reads lines laid out by kOutputTemplate; other lines (progress) are ignored.
        [[nodiscard]] static std::vector<Diagnostic> parseDiagnostics(const std::string& output);
        void execute(const std::string& file, const ctrace::ProgramConfig& config,
                     ToolOutput& output) const override;
        std::string name() const override;
    };

} // namespace ctrace

#endif // ANALYSIS_TOOLS_HPP
