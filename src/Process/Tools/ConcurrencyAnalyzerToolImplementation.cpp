// SPDX-License-Identifier: Apache-2.0
#include "Process/Tools/AnalysisTools.hpp"
#include "Process/Tools/CompileCommands.hpp"
#include "Process/Tools/ConcurrencyAnalyzerBridge.hpp"
#include "Process/Tools/InProcessAnalysis.hpp"
#include "Process/Tools/InputPaths.hpp"

#include <coretrace/logger.hpp>
#include <coretrace_concurrency_analysis.hpp>
#include <coretrace_concurrency_analyzer.hpp>

#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/raw_ostream.h>

#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    constexpr std::string_view kToolName = "coretrace-concurrency-analyzer";

    /// Compiles `file` to IR in `context`. A source that does not compile is reported on
    /// `output`, with the compiler's own diagnostics, and yields nothing.
    [[nodiscard]] std::optional<ctrace::concurrency::CompileResult>
    compileUnit(const ctrace::concurrency::InMemoryIRCompiler& compiler, const std::string& file,
                std::vector<std::string> arguments, llvm::LLVMContext& context,
                ctrace::ToolOutput& output)
    {
        ctrace::concurrency::CompileRequest request;
        request.inputFile = file;
        request.extraCompileArgs = std::move(arguments);
        // In memory, as the stack analyzer compiles: coretrace-compiler then runs cc1 in this
        // process. Its bitcode-file output goes through the clang driver, which runs a clang
        // executable, and a release archive ships none (CT_CLANG names ctrace itself).
        request.format = ctrace::concurrency::IRFormat::LL;
        ctrace::concurrency::CompileResult result = compiler.compile(request, context);
        if (result.success && result.module != nullptr)
        {
            return result;
        }
        const std::string why = result.diagnostics.empty()
                                    ? ctrace::concurrency::formatCompileError(result.error)
                                    : result.diagnostics;
        output.error("Unable to compile " + file + " for the concurrency analysis:\n" + why);
        return std::nullopt;
    }

    /// `module` as bitcode, the form a project analysis reads its units from.
    [[nodiscard]] std::string bitcodeOf(const llvm::Module& module)
    {
        std::string bitcode;
        llvm::raw_string_ostream stream(bitcode);
        llvm::WriteBitcodeToFile(module, stream);
        stream.flush();
        return bitcode;
    }

    void collect(std::vector<ctrace::Diagnostic>& into,
                 const ctrace::concurrency::DiagnosticReport& report,
                 const ctrace::InputPaths& paths)
    {
        for (const ctrace::concurrency::Diagnostic& finding : report.diagnostics)
        {
            into.push_back(ctrace::toDiagnostic(finding, std::string(kToolName), paths));
        }
    }

    /// Each input on its own, built with the configured context. Nothing when no input could
    /// be analyzed at all.
    [[nodiscard]] std::optional<std::vector<ctrace::Diagnostic>>
    analyzeEachUnit(const std::vector<std::string>& files, const ctrace::ProgramConfig& config,
                    const ctrace::concurrency::AnalysisOptions& options, ctrace::ToolOutput& output)
    {
        const ctrace::InputPaths paths(files);
        std::vector<std::string> context;
        ctrace::appendBuildContext(context, config);
        const ctrace::concurrency::InMemoryIRCompiler compiler;
        const ctrace::concurrency::SingleTUConcurrencyAnalyzer analyzer(options);
        std::vector<ctrace::Diagnostic> diagnostics;
        bool analyzed = false;
        for (const std::string& file : files)
        {
            llvm::LLVMContext llvmContext;
            const auto compiled = compileUnit(compiler, file, context, llvmContext, output);
            if (!compiled)
            {
                continue;
            }
            collect(diagnostics, analyzer.analyze(*compiled->module), paths);
            analyzed = true;
        }
        if (!analyzed)
        {
            return std::nullopt;
        }
        return diagnostics;
    }

    /// The inputs as one program, each unit built as `database` says. A unit the database does
    /// not list, or that does not compile, is reported and left out: the program's analysis
    /// is then incomplete, never clean. Nothing when no unit could be built.
    [[nodiscard]] std::optional<std::vector<ctrace::Diagnostic>>
    analyzeProject(const std::vector<std::string>& files, const std::filesystem::path& database,
                   const ctrace::concurrency::AnalysisOptions& options, ctrace::ToolOutput& output)
    {
        std::string error;
        const auto commands = ctrace::readCompileCommands(database, error);
        if (!commands)
        {
            output.error(error);
            return std::nullopt;
        }

        std::map<std::filesystem::path, const ctrace::CompileCommand*> byFile;
        for (const ctrace::CompileCommand& command : *commands)
        {
            byFile.emplace(ctrace::InputPaths::identityOf(command.file), &command);
        }

        const ctrace::InputPaths paths(files);
        const ctrace::concurrency::InMemoryIRCompiler compiler;
        ctrace::concurrency::ProjectUnitSource units;
        for (const std::string& file : files)
        {
            const auto command = byFile.find(ctrace::InputPaths::identityOf(file));
            if (command == byFile.end())
            {
                output.error(file + " is not in " + database.string() +
                             ": it cannot be built as part of the program, so its concurrency "
                             "analysis is missing.");
                continue;
            }
            llvm::LLVMContext llvmContext;
            const auto compiled =
                compileUnit(compiler, file, command->second->arguments, llvmContext, output);
            if (!compiled)
            {
                continue;
            }
            // Held as bitcode, and parsed again when the analysis reaches the unit, so that only
            // the units it works on are in memory at once.
            units.add(file, bitcodeOf(*compiled->module));
        }
        if (units.size() == 0)
        {
            return std::nullopt;
        }

        const ctrace::concurrency::ProjectAnalysisReport analysis =
            ctrace::concurrency::ProjectConcurrencyAnalyzer(options).analyze(units);
        for (const ctrace::concurrency::FailedUnit& unit : analysis.failedUnits)
        {
            output.error("Unable to analyze " + unit.identifier + ": " + unit.error.message);
        }
        for (const std::string& module : analysis.skippedIncompatibleModules)
        {
            output.error(module + " targets another ABI than the rest of the program and was "
                                  "left out of the concurrency analysis.");
        }
        std::vector<ctrace::Diagnostic> diagnostics;
        collect(diagnostics, analysis.report, paths);
        return diagnostics;
    }
} // namespace

namespace ctrace
{
    void ConcurrencyAnalyzerToolImplementation::executeBatch(const std::vector<std::string>& files,
                                                             const ProgramConfig& config,
                                                             ToolOutput& output) const
    {
        std::string error;
        const auto options = concurrencyRules(config.tools.concurrency_analyzer_rules, error);
        if (!options)
        {
            output.error(error);
            return;
        }

        const std::filesystem::path database = compileCommandsPath(config);
        if (database.empty())
        {
            coretrace::log(coretrace::Level::Info, "Running {} on {} file(s)\n", kToolName,
                           files.size());
        }
        else
        {
            coretrace::log(coretrace::Level::Info,
                           "Running {} on {} file(s) as one program, built as {} says\n", kToolName,
                           files.size(), database.string());
        }

        const std::lock_guard<std::mutex> serializedRun(inProcessAnalysisMutex());
        auto diagnostics = database.empty() ? analyzeEachUnit(files, config, *options, output)
                                            : analyzeProject(files, database, *options, output);
        if (!diagnostics)
        {
            return;
        }
        // In SARIF mode the merged document is the output; text lines would pollute it.
        if (!config.output.sarif_format && !diagnostics->empty())
        {
            output.result(renderLines(*diagnostics));
        }
        output.diagnostics(std::move(*diagnostics));
    }

    std::string ConcurrencyAnalyzerToolImplementation::name() const
    {
        return std::string(kToolName);
    }
} // namespace ctrace
