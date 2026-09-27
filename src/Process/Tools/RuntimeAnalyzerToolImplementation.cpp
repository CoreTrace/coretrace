// SPDX-License-Identifier: Apache-2.0
#include "Process/Tools/AnalysisTools.hpp"
#include "Process/Tools/InputPaths.hpp"
#include "Process/Tools/Sarif.hpp"

#include <coretrace/logger.hpp>

#include <cerrno>
#include <cstring>
#include <system_error>

#include <unistd.h>

namespace ctrace
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr const char* kToolName = "coretrace-runtime-analyzer";

        /// The analyzer's exit codes that mean it built and ran the program: 0 clean, 1
        /// findings. 2 means it could not, and the analysis of that program is incomplete.
        constexpr std::initializer_list<int> kCompletedExitCodes = {0, 1};

        /// ctrace stops the tool once it has taken this many times the program's own timeout:
        /// building a single program gets as long as running it.
        constexpr unsigned kDeadlineFactor = 2;

        /// A directory of its own for one program's build, removed with this object: runs in
        /// parallel never share a path, and nothing lands in the working directory.
        class RunDirectory
        {
          public:
            RunDirectory()
            {
                std::string pattern =
                    (fs::temp_directory_path() / "ctrace-runtime-XXXXXX").string();
                if (::mkdtemp(pattern.data()) == nullptr)
                {
                    throw std::runtime_error("cannot create a run directory: " +
                                             std::string(std::strerror(errno)));
                }
                path_ = pattern;
            }

            ~RunDirectory()
            {
                std::error_code err;
                fs::remove_all(path_, err);
            }

            RunDirectory(const RunDirectory&) = delete;
            RunDirectory& operator=(const RunDirectory&) = delete;

            [[nodiscard]] const fs::path& path() const noexcept
            {
                return path_;
            }

          private:
            fs::path path_;
        };
    } // namespace

    std::vector<std::string> RuntimeAnalyzerToolImplementation::buildArguments(
        const ProgramConfig& config, const std::string& file, const fs::path& program)
    {
        // Always SARIF: CoreTrace renders the text itself and merges SARIF across tools.
        std::vector<std::string> args = {
            "--format",  "sarif",
            "--timeout", std::to_string(config.tools.runtime_analyzer_timeout_s),
            "-o",        program.string()};
        appendToolArguments(args, config, kToolName);
        // After `--`, the compiler's arguments: the build context CoreTrace knows, then the
        // program's source.
        args.emplace_back("--");
        for (const std::string& dir : config.stack_analyzer.include_dirs)
        {
            args.push_back("-I" + dir);
        }
        for (const std::string& define : config.stack_analyzer.defines)
        {
            args.push_back("-D" + define);
        }
        args.push_back(file);
        return args;
    }

    void RuntimeAnalyzerToolImplementation::execute(const std::string& file,
                                                    const ProgramConfig& config,
                                                    ToolOutput& output) const
    {
        coretrace::log(coretrace::Level::Info, "Running {} on {}\n", kToolName, file);
        const RunDirectory runDirectory;
        const std::chrono::seconds deadline(
            std::chrono::seconds::rep{config.tools.runtime_analyzer_timeout_s} * kDeadlineFactor);
        const auto run = runExternalTool(
            config, *this, buildArguments(config, file, runDirectory.path() / "program"), output,
            kCompletedExitCodes, deadline);
        if (!run)
        {
            return;
        }

        auto diagnostics = diagnosticsFromSarifText(run->output, kToolName);
        if (!diagnostics)
        {
            // A report is the tool's answer when it built and ran the program; without one,
            // its exit code says nothing about the program.
            if (run->completedWith(kCompletedExitCodes))
            {
                output.error(name() + " did not produce a SARIF report (exit code " +
                             std::to_string(run->exitCode) + "); the analysis is incomplete");
            }
            output.record("stdout", run->output);
            return;
        }
        // The analyzer names each finding's file as the source was given to it: the input.
        const InputPaths paths({file});
        for (Diagnostic& diagnostic : *diagnostics)
        {
            diagnostic.file = paths.display(diagnostic.file);
        }
        // In SARIF mode the merged document is the output; text lines would pollute it.
        if (!config.output.sarif_format && !diagnostics->empty())
        {
            output.result(renderLines(*diagnostics));
        }
        output.diagnostics(*diagnostics);
    }

    std::string RuntimeAnalyzerToolImplementation::name() const
    {
        return kToolName;
    }
} // namespace ctrace
