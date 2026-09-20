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
            args.push_back(file);
            return args;
        }

        void execute(const std::string& file, const ctrace::ProgramConfig& config,
                     ToolOutput& output) const override
        {
            coretrace::log(coretrace::Level::Info, "Running ikos on {}\n", file);

            try
            {
                const std::vector<std::string> argsProcess = buildArguments(config, file);
                auto process =
                    ProcessFactory::createProcess("./ikos/src/ikos-build/bin/ikos", argsProcess);
                // std::this_thread::sleep_for(std::chrono::seconds(5));
                const ProcessResult run = process->execute();
                output.result(run.output);
                if (!run.succeeded())
                {
                    output.error(run.describeFailure(name()));
                }
            }
            catch (const std::exception& e)
            {
                output.error("Error: " + std::string(e.what()));
            }
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
        [[nodiscard]] DiagnosticSummary lastDiagnosticsSummary() const override;
        std::string name() const override;

      private:
        mutable DiagnosticSummary m_lastDiagnosticsSummary{};
    };

    class FlawfinderToolImplementation : public AnalysisToolBase
    {
      public:
        [[nodiscard]] static std::vector<std::string>
        buildArguments(const ctrace::ProgramConfig& config, const std::string& file)
        {
            std::vector<std::string> args = {
                "./flawfinder/src/flawfinder-build/flawfinder.py",
                "-c",
                "-C",
                "-D",
            };
            if (config.output.sarif_format)
            {
                args.push_back("--sarif");
            }
            args.push_back(file);
            return args;
        }

        void execute(const std::string& file, const ctrace::ProgramConfig& config,
                     ToolOutput& output) const override
        {
            coretrace::log(coretrace::Level::Info, "Running flawfinder on {}\n", file);

            try
            {
                const std::vector<std::string> argsProcess = buildArguments(config, file);
                auto process = ProcessFactory::createProcess("python3", argsProcess);
                const ProcessResult run = process->execute();

                if (config.runtime.ipc == "standardIO")
                {
                    output.result(run.output);
                }
                else
                {
                    ipc->write(run.output);
                }
                if (!run.succeeded())
                {
                    output.error(run.describeFailure(name()));
                }
            }
            catch (const std::exception& e)
            {
                output.error("Error: " + std::string(e.what()));
                return;
            }
        }
        std::string name() const override
        {
            return "flawfinder";
        }
    };

    class TscancodeToolImplementation : public AnalysisToolBase
    {
      public:
        [[nodiscard]] static std::vector<std::string> buildArguments(const ProgramConfig& config,
                                                                     const std::string& file);
        void execute(const std::string& file, const ProgramConfig& config,
                     ToolOutput& output) const override;
        std::string name() const override;

        /// Converts tscancode's text diagnostics into a SARIF document. Pure: the caller
        /// decides where the document goes.
        [[nodiscard]] nlohmann::json sarifFormat(const std::string& buffer) const;

      protected:
        [[nodiscard]] std::string_view severityToLevel(const std::string& severity) const;
    };

    class CppCheckToolImplementation : public AnalysisToolBase
    {
      public:
        [[nodiscard]] static std::vector<std::string>
        buildArguments(const ctrace::ProgramConfig& config, const std::string& file)
        {
            std::vector<std::string> args;
            if (config.output.sarif_format)
            {
                args.push_back("--output-format=sarif");
            }
            args.push_back(file);
            return args;
        }

        void execute(const std::string& file, const ctrace::ProgramConfig& config,
                     ToolOutput& output) const override
        {
            coretrace::log(coretrace::Level::Info, "Running cppcheck on {}\n", file);

            try
            {
                const std::vector<std::string> argsProcess = buildArguments(config, file);
                auto process =
                    ProcessFactory::createProcess("/opt/homebrew/bin/cppcheck", argsProcess);
                const ProcessResult run = process->execute();
                output.result(run.output);
                if (!run.succeeded())
                {
                    output.error(run.describeFailure(name()));
                }
            }
            catch (const std::exception& e)
            {
                output.error("Error: " + std::string(e.what()));
                return;
            }
        }
        std::string name() const override
        {
            return "cppcheck";
        }
    };

    // Outils dynamiques
    class DynTool1 : public AnalysisToolBase
    {
      public:
        void execute(const std::string& file, const ctrace::ProgramConfig& config,
                     ToolOutput& output) const override
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
        void execute(const std::string& file, const ctrace::ProgramConfig& config,
                     ToolOutput& output) const override
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
        void execute(const std::string& file, const ctrace::ProgramConfig& config,
                     ToolOutput& output) const override
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
