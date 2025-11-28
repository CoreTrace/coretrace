#ifndef ANALYSIS_TOOLS_HPP
#define ANALYSIS_TOOLS_HPP

#include <regex>

// #include "IAnalysisTools.hpp"
#include "AnalysisToolsBase.hpp"
#include "ctrace_tools/languageType.hpp"
#include "ctrace_tools/mangle.hpp"
#include "../ProcessFactory.hpp"
#include "../ThreadProcess.hpp"
#include <nlohmann/json.hpp>

#include "StackUsageAnalyzer.hpp"
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

using json = nlohmann::json;

class EntryPoint
{
    public:
        EntryPoint(const std::string& entryPointName, const std::vector<std::string>& paramTypes)
            : name(entryPointName), paramTypes(paramTypes)
        {
            m_isMangled = ctrace_tools::isMangled(name);

            if (m_isMangled)
            {
                mangledName = name;
            }
            else
            {
                mangledName = ctrace_tools::mangleFunction("", name, paramTypes);
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
        void execute(const std::string& file, ctrace::ProgramConfig config) const override
        {
            ctrace::Thread::Output::cout("Running IKOS on " + file);
            std::string src_file = file;
            std::string entry_points = config.global.entry_points;
            std::string report_file = config.global.report_file;


            try {
                std::vector<std::string> argsProcess;

                if (config.global.hasSarifFormat)
                {
                    argsProcess.push_back("--format=text");
                }
                argsProcess.push_back("-a=upa,dfa,pcmp,poa,nullity,fca");
                argsProcess.push_back("-d=congruence");
                // argsProcess.push_back("--rm-db");
                argsProcess.push_back("--partitioning=return");

                ctrace_defs::LanguageType lang = ctrace_tools::detectLanguage(src_file);

                if (lang == ctrace_defs::LanguageType::C)
                {
                    // std::cout << "C file detected\n";
                    ctrace::Thread::Output::cout("C file detected");
                    EntryPoint entryPoint(entry_points, {"void"}); // TODO parse function parameters
                    ctrace::Thread::Output::cout("Entry point: " + std::string(entryPoint.getEntryPointNameCMode()));
                    std::string arg = "--entry-points=";
                    arg += entryPoint.getEntryPointNameCMode();
                    argsProcess.push_back(arg);
                }
                else if (lang == ctrace_defs::LanguageType::CPP)
                {
                    ctrace::Thread::Output::cout("C++ file detected");
                    EntryPoint entryPoint(entry_points, {"void"}); // TODO parse function parameters
                    ctrace::Thread::Output::cout("Entry point: " + std::string(entryPoint.getEntryPointNameCCMode()));
                    std::string arg = "--entry-points=";
                    arg += entryPoint.getEntryPointNameCCMode();
                    argsProcess.push_back(arg);
                }
                argsProcess.push_back("--report-file=" + report_file);
                argsProcess.push_back(src_file);

                auto process = ProcessFactory::createProcess("./ikos/src/ikos-build/bin/ikos", argsProcess); // ou "cmd.exe" pour Windows
                // std::this_thread::sleep_for(std::chrono::seconds(5));
                process->execute();
                ctrace::Thread::Output::cout(process->logOutput);

            } catch (const std::exception& e) {
                ctrace::Thread::Output::cout("Error: " + std::string(e.what()));
                // return 1;
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
        void execute(const std::string& file, ctrace::ProgramConfig config) const override;
        std::string name() const override;
};

class FlawfinderToolImplementation : public AnalysisToolBase
{
    public:
        void execute(const std::string& file, ctrace::ProgramConfig config) const override
        {
            ctrace::Thread::Output::cout("Running flawfinder on " + file);

            bool has_sarif_format = config.global.hasSarifFormat;
            std::string src_file = file;
            std::string entry_points = config.global.entry_points;

            try {
                std::vector<std::string> argsProcess;
                // = {"flawfinder.py", "-F", "-c", "-C", "-D", "main.c"};
                argsProcess.push_back("./flawfinder/src/flawfinder-build/flawfinder.py");
                // argsProcess.push_back("-F");
                argsProcess.push_back("-c");
                argsProcess.push_back("-C");
                argsProcess.push_back("-D");
                if (has_sarif_format)
                {
                    argsProcess.push_back("--sarif");
                }
                argsProcess.push_back(src_file);
                auto process = ProcessFactory::createProcess("python3", argsProcess); // or "cmd.exe" for Windows
                process->execute();

                if (config.global.ipc == "standardIO")
                {
                    ctrace::Thread::Output::cout(process->logOutput);
                }
                else
                {
                    ipc->write(process->logOutput);
                }

            } catch (const std::exception& e) {
                ctrace::Thread::Output::cout("Error: " + std::string(e.what()));
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
    void execute(const std::string& file, ProgramConfig config) const override;
    std::string name() const override;

protected:
    std::string_view severityToLevel(const std::string& severity) const;
    json sarifFormat(const std::string &buffer, const std::string &outputFile) const;
    json jsonFormat(const std::string &buffer, const std::string &outputFile) const;
};


class CppCheckToolImplementation : public AnalysisToolBase
{
    public:
        void execute(const std::string& file, ctrace::ProgramConfig config) const override
        {
            ctrace::Thread::Output::cout("Running ikos on " + file);
            bool has_sarif_format = config.global.hasSarifFormat;
            std::string src_file = file;
            std::string entry_points = config.global.entry_points;

            try {
                std::vector<std::string> argsProcess;

                if (has_sarif_format)
                {
                    argsProcess.push_back("--output-format=sarif");
                }
                // argsProcess.push_back("--enable=all");
                argsProcess.push_back(src_file);

                auto process = ProcessFactory::createProcess("/opt/homebrew/bin/cppcheck", argsProcess); // ou "cmd.exe" pour Windows
                process->execute();
                ctrace::Thread::Output::cout(process->logOutput);
            } catch (const std::exception& e) {
                ctrace::Thread::Output::cout("Error: " + std::string(e.what()));
                return;
            }
        }
        std::string name() const override
        {
            return "ikos";
        }
};

    // Outils dynamiques
class DynTool1 : public AnalysisToolBase
{
    public:
        void execute(const std::string& file, ctrace::ProgramConfig config) const override
        {
            ctrace::Thread::Output::cout("Running dyn_tools_1 on " + file);
        }
        std::string name() const override
        {
            return "dyn_tools_1";
        }
};

class DynTool2 : public AnalysisToolBase
{
    public:
        void execute(const std::string& file, ctrace::ProgramConfig config) const override
        {
            ctrace::Thread::Output::cout("Running dyn_tools_2 on " + file);
        }
        std::string name() const override
        {
            return "dyn_tools_2";
        }
};

class DynTool3 : public AnalysisToolBase
{
    public:
        void execute(const std::string& file, ctrace::ProgramConfig config) const override
        {
            ctrace::Thread::Output::cout("Running dyn_tools_3 on " + file);
        }
        std::string name() const override
        {
            return "dyn_tools_3";
        }
};

}

#endif // ANALYSIS_TOOLS_HPP
