// SPDX-License-Identifier: Apache-2.0
#ifndef ANALYSIS_TOOLS_HPP
#define ANALYSIS_TOOLS_HPP

#include "AnalysisToolsBase.hpp"
#include "ctrace_tools/languageType.hpp"
#include "ctrace_tools/mangle.hpp"
#include "../ProcessFactory.hpp"
#include "ToolOutput.hpp"

#include <coretrace/logger.hpp>
#include <nlohmann/json.hpp>

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

        if (m_isMangled)
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
    [[nodiscard]] std::vector<std::string> getParamTypes() const noexcept
    {
        if (paramTypes.empty())
        {
            return {"void"};
        }
        return paramTypes;
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

    /// Runs an external tool to completion. A tool that cannot be started is reported on the
    /// sink and yields nothing. An exit code outside `completedCodes` (by default only 0) is
    /// reported too, but the output is still returned: partial findings are not lost because
    /// the tool ended badly.
    [[nodiscard]] inline std::optional<ProcessResult>
    runExternalTool(const ProgramConfig& config, const IAnalysisTool& tool,
                    const std::vector<std::string>& args, ToolOutput& output,
                    std::initializer_list<int> completedCodes = {0})
    {
        try
        {
            auto process = ProcessFactory::createProcess(toolCommand(config, tool), args);
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
        [[nodiscard]] static std::vector<std::string>
        buildArguments(const ctrace::ProgramConfig& config, const std::string& file)
        {
            std::vector<std::string> args;
            args.push_back(config.output.sarif_format ? "--format=json" : "--format=text");
            args.push_back("-a=upa,dfa,pcmp,poa,nullity,fca");
            args.push_back("-d=congruence");
            args.push_back("--partitioning=return");

            const std::string entry_points =
                ctrace_tools::strings::joinByComma(config.files.entry_points);
            const EntryPoint entryPoint(entry_points, {"void"}); // TODO parse function parameters
            const bool isC = ctrace_tools::detectLanguage(file) == ctrace_defs::LanguageType::C;
            args.push_back("--entry-points=" +
                           std::string(isC ? entryPoint.getEntryPointNameCMode()
                                           : entryPoint.getEntryPointNameCCMode()));
            args.push_back("--report-file=" + config.output.report_file);
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
            const auto run = runExternalTool(config, *this, buildArguments(config, file), output);
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
    /// the Python inputs, and only the inputs' findings are kept. Its exit code carries a
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
        /// `inputs`, spelled as the input was. Nothing when the output is not SARIF.
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

    // Outils dynamiques
    class DynTool1 : public AnalysisToolBase
    {
      public:
        void execute(const std::string& file, const ctrace::ProgramConfig& /*config*/,
                     ToolOutput& /*output*/) const override
        {
            coretrace::log(coretrace::Level::Info, "Running dyn_tools_1 on {}\n", file);
        }
        std::string name() const override
        {
            return "dyn_tools_1";
        }
    };

    class DynTool2 : public AnalysisToolBase
    {
      public:
        void execute(const std::string& file, const ctrace::ProgramConfig& /*config*/,
                     ToolOutput& /*output*/) const override
        {
            coretrace::log(coretrace::Level::Info, "Running dyn_tools_2 on {}\n", file);
        }
        std::string name() const override
        {
            return "dyn_tools_2";
        }
    };

    class DynTool3 : public AnalysisToolBase
    {
      public:
        void execute(const std::string& file, const ctrace::ProgramConfig& /*config*/,
                     ToolOutput& /*output*/) const override
        {
            coretrace::log(coretrace::Level::Info, "Running dyn_tools_3 on {}\n", file);
        }
        std::string name() const override
        {
            return "dyn_tools_3";
        }
    };

} // namespace ctrace

#endif // ANALYSIS_TOOLS_HPP
