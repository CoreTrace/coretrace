#include "Process/Tools/AnalysisTools.hpp"

namespace ctrace
{
void StackAnalyzerToolImplementation::execute(const std::string& file, ctrace::ProgramConfig config) const
{
    ctrace::Thread::Output::cout("Running CoreTrace Stack Usage Analyzer on " + file);
    std::string filename = file;

    llvm::LLVMContext ctx;
    llvm::SMDiagnostic diag;

    ctrace::stack::AnalysisConfig cfg;
    cfg.mode = ctrace::stack::AnalysisMode::IR;
    cfg.stackLimit = 8 * 1024 * 1024;

    auto res = ctrace::stack::analyzeFile(filename, cfg, ctx, diag);

    if (config.global.ipc == "standardIO")
    {
        if (config.global.hasSarifFormat)
        {
            ctrace::Thread::Output::tool_out(ctrace::stack::toJson(res, filename));
            return;
        }
            if (res.functions.empty()) {
                // std::err.print("stack_usage_analyzer", llvm::errs());
                return;
            }
            llvm::outs() << "Mode: "
                        << (res.config.mode == ctrace::stack::AnalysisMode::IR ? "IR" : "ABI")
                        << "\n\n";

            for (const auto &f : res.functions) {
                std::vector<std::string> param_types;
                // param_types.reserve(issue.inst->getFunction()->arg_size());
                param_types.push_back("void"); // dummy to avoid empty vector issue // refaire avec les paramèters réels

                // llvm::outs() << "Function: " << f.name << " " << ((ctrace::stack::isMangled(f.name)) ? ctrace::stack::demangle(f.name.c_str()) : "") <<  "\n";
                llvm::outs() << "Function: " << f.name << " " << "\n";
                llvm::outs() << "  local stack: " << f.localStack << " bytes\n";
                llvm::outs() << "  max stack (including callees): " << f.maxStack << " bytes\n";

                if (f.isRecursive)
                    llvm::outs() << "  [!] recursive or mutually recursive function detected\n";

                if (f.hasInfiniteSelfRecursion)
                {
                    llvm::outs() << "  [!!!] unconditional self recursion detected (no base case)\n";
                    llvm::outs() << "       this will eventually overflow the stack at runtime\n";
                }

                if (f.exceedsLimit)
                {
                    llvm::outs() << "  [!] potential stack overflow: exceeds limit of "
                                << res.config.stackLimit << " bytes\n";
                }

                if (!res.config.quiet)
                {
                    for (const auto &d : res.diagnostics)
                    {
                        if (d.funcName != f.name)
                            continue;

                        if (res.config.warningsOnly
                        && d.severity == ctrace::stack::DiagnosticSeverity::Info)
                            continue;

                        if (d.line != 0)
                        {
                            llvm::outs() << "  at line " << d.line
                            << ", column " << d.column << "\n";
                        }
                        llvm::outs() << d.message << "\n";
                    }
                }

                llvm::outs() << "\n";
            }
    }
    if (config.global.ipc == "socket")
    {
        ipc->write(ctrace::stack::toJson(res, filename));
    }
}

std::string StackAnalyzerToolImplementation::name() const
{
    return "ctrace_stack_analyzer";
}

}
