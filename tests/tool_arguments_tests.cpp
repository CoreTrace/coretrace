// SPDX-License-Identifier: Apache-2.0
//
// Each tool reports its own identity and builds its command line from the configuration.
#include "Process/Tools/AnalysisTools.hpp"
#include "Process/Tools/CompileCommands.hpp"
#include "ctrace_tools/languageType.hpp"

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    struct TestReport
    {
        int failures = 0;

        void expect(bool condition, const std::string& message)
        {
            if (condition)
            {
                std::cout << "[PASS] " << message << "\n";
                return;
            }
            ++failures;
            std::cerr << "[FAIL] " << message << "\n";
        }
    };

    bool contains(const std::vector<std::string>& args, const std::string& value)
    {
        return std::find(args.begin(), args.end(), value) != args.end();
    }

    ctrace::ProgramConfig configWithSarif(bool sarif)
    {
        ctrace::ProgramConfig config;
        config.output.sarif_format = sarif;
        config.files.entry_points = {"main"};
        return config;
    }
} // namespace

int main()
{
    using namespace ctrace;
    TestReport report;

    // A tool's name identifies it in logs, diagnostics summaries and the server response.
    report.expect(CppCheckToolImplementation().name() == "cppcheck",
                  "cppcheck reports its own name");
    report.expect(IkosToolImplementation().name() == "ikos", "ikos reports its own name");
    report.expect(FlawfinderToolImplementation().name() == "flawfinder",
                  "flawfinder reports its own name");
    report.expect(TscancodeToolImplementation().name() == "tscancode",
                  "tscancode reports its own name");

    // --sarif-format asks each tool for its structured output, never for text.
    {
        const auto args = IkosToolImplementation::buildArguments(configWithSarif(true), "a.c");
        report.expect(contains(args, "--format=json") && !contains(args, "--format=text"),
                      "ikos: a SARIF request asks for the structured format");
        report.expect(contains(args, "a.c"), "ikos: the source file is passed");
    }
    {
        const auto args = IkosToolImplementation::buildArguments(configWithSarif(false), "a.c");
        report.expect(contains(args, "--format=text") && !contains(args, "--format=json"),
                      "ikos: without SARIF the text format is used");
    }
    // ikos names entry points as the binary does: plain in C, Itanium-mangled in C++ except
    // main, which C++ never mangles, and an already mangled name is kept.
    {
        report.expect(
            contains(IkosToolImplementation::buildArguments(configWithSarif(false), "a.c"),
                     "--entry-points=main"),
            "ikos: a C entry point keeps its name");
        report.expect(
            contains(IkosToolImplementation::buildArguments(configWithSarif(false), "a.cpp"),
                     "--entry-points=main"),
            "ikos: the C++ main entry point is not mangled");
        ctrace::ProgramConfig function = configWithSarif(false);
        function.files.entry_points = {"run"};
        report.expect(contains(IkosToolImplementation::buildArguments(function, "a.cpp"),
                               "--entry-points=_Z3runv"),
                      "ikos: a C++ entry point is mangled");
        ctrace::ProgramConfig mangled = configWithSarif(false);
        mangled.files.entry_points = {"_Z3runi"};
        report.expect(contains(IkosToolImplementation::buildArguments(mangled, "a.cpp"),
                               "--entry-points=_Z3runi"),
                      "ikos: an already mangled C++ entry point is kept");

        // Each entry point is named on its own, then the names are joined.
        ctrace::ProgramConfig several = configWithSarif(false);
        several.files.entry_points = {"main", "foo", "_Z3runi"};
        report.expect(contains(IkosToolImplementation::buildArguments(several, "a.cpp"),
                               "--entry-points=main,_Z3foov,_Z3runi"),
                      "ikos: several C++ entry points are mangled one by one");
        report.expect(contains(IkosToolImplementation::buildArguments(several, "a.c"),
                               "--entry-points=main,foo,_Z3runi"),
                      "ikos: several C entry points keep their names");
    }
    {
        // The merged SARIF document is rendered by CoreTrace from the model, so cppcheck
        // always runs with the template CoreTrace parses, whatever the output mode.
        const auto args = CppCheckToolImplementation::buildArguments(configWithSarif(true), "a.c");
        report.expect(!contains(args, "--output-format=sarif") &&
                          contains(args, std::string("--template=") +
                                             CppCheckToolImplementation::kOutputTemplate),
                      "cppcheck: a SARIF request keeps the parsed text template");
        const auto text = CppCheckToolImplementation::buildArguments(configWithSarif(false), "a.c");
        // The template is the parsing contract: the same on every cppcheck version.
        report.expect(contains(text, std::string("--template=") +
                                         CppCheckToolImplementation::kOutputTemplate) &&
                          text.back() == "a.c",
                      "cppcheck: text runs use the explicit output template, file last");
    }
    {
        const auto args =
            FlawfinderToolImplementation::buildArguments(configWithSarif(true), "a.c");
        report.expect(contains(args, "--sarif") && args.back() == "a.c",
                      "flawfinder: SARIF request is forwarded and the file comes last");
        report.expect(
            contains(FlawfinderToolImplementation::buildArguments(configWithSarif(false), "a.c"),
                     "--sarif"),
            "flawfinder: the structured output is always requested; CoreTrace renders the text");
        report.expect(std::none_of(args.begin(), args.end(), [](const std::string& arg)
                                   { return arg.find(".py") != std::string::npos; }),
                      "flawfinder: no script path in the arguments");
    }

    // cppcheck gets the checks and the build context the configuration already knows about.
    {
        const auto args = CppCheckToolImplementation::buildArguments(configWithSarif(false), "a.c");
        report.expect(contains(args, "--enable=warning,style,performance,portability"),
                      "cppcheck: warning, style, performance and portability checks are enabled");
        report.expect(contains(args, "--inline-suppr"),
                      "cppcheck: inline suppression comments are honoured");
        report.expect(!contains(args, "-j") && args.back() == "a.c",
                      "cppcheck: no -j without a numeric jobs value; file last");

        ctrace::ProgramConfig config = configWithSarif(false);
        config.stack_analyzer.include_dirs = {"inc", "third_party"};
        config.stack_analyzer.defines = {"FOO=1", "BAR"};
        config.stack_analyzer.jobs = "4";
        const auto derived = CppCheckToolImplementation::buildArguments(config, "a.c");
        report.expect(contains(derived, "-Iinc") && contains(derived, "-Ithird_party"),
                      "cppcheck: include_dirs become -I");
        report.expect(contains(derived, "-DFOO=1") && contains(derived, "-DBAR"),
                      "cppcheck: defines become -D");
        const auto jobs = std::find(derived.begin(), derived.end(), "-j");
        report.expect(jobs != derived.end() && std::next(jobs) != derived.end() &&
                          *std::next(jobs) == "4",
                      "cppcheck: a numeric jobs value becomes -j N");
        config.stack_analyzer.jobs = "auto";
        report.expect(!contains(CppCheckToolImplementation::buildArguments(config, "a.c"), "-j"),
                      "cppcheck: jobs=auto is not forwarded (cppcheck needs a number)");
    }

    // Per-tool pass-through arguments come after the derived options, so they can override
    // them, and before the file.
    {
        ctrace::ProgramConfig config = configWithSarif(false);
        config.tools.args["cppcheck"] = {"--disable=style", "--std=c++20"};
        const auto args = CppCheckToolImplementation::buildArguments(config, "a.c");
        const auto enable = std::find_if(args.begin(), args.end(), [](const std::string& arg)
                                         { return arg.rfind("--enable=", 0) == 0; });
        const auto disable = std::find(args.begin(), args.end(), "--disable=style");
        report.expect(
            enable != args.end() && disable != args.end() && enable < disable &&
                contains(args, "--std=c++20") && args.back() == "a.c",
            "cppcheck: tools.cppcheck.args follow the derived options and precede the file");

        config.tools.args["flawfinder"] = {"--minlevel=3"};
        config.tools.args["tscancode"] = {"--xml"};
        config.tools.args["ikos"] = {"--opt=1"};
        const auto flaw = FlawfinderToolImplementation::buildArguments(config, "a.c");
        const auto tscan = TscancodeToolImplementation::buildArguments(config, "a.c");
        const auto ikos = IkosToolImplementation::buildArguments(config, "a.c");
        report.expect(contains(flaw, "--minlevel=3") && flaw.back() == "a.c",
                      "flawfinder: pass-through arguments precede the file");
        report.expect(contains(tscan, "--xml") && tscan.back() == "a.c",
                      "tscancode: pass-through arguments precede the file");
        report.expect(contains(ikos, "--opt=1") && ikos.back() == "a.c",
                      "ikos: pass-through arguments precede the file");
        report.expect(
            !contains(CppCheckToolImplementation::buildArguments(configWithSarif(false), "a.c"),
                      "--disable=style"),
            "pass-through arguments are per tool and off by default");
    }

    // Each file goes to the tools of its language: .py is Python, never C++ by default.
    {
        using ctrace_defs::LanguageType;
        using ctrace_tools::detectLanguage;
        report.expect(detectLanguage("pkg/app.py") == LanguageType::Python,
                      "detectLanguage: .py is Python");
        report.expect(detectLanguage("a.c") == LanguageType::C &&
                          detectLanguage("a.cc") == LanguageType::CPP &&
                          detectLanguage("a.hpp") == LanguageType::CPP,
                      "detectLanguage: C and C++ extensions are unchanged");

        const PythonAnalyzerToolImplementation python;
        const CppCheckToolImplementation cppcheck;
        report.expect(python.analyzes(LanguageType::Python) && !python.analyzes(LanguageType::C) &&
                          !python.analyzes(LanguageType::CPP),
                      "coretrace-python-analyzer analyzes Python only");
        report.expect(cppcheck.analyzes(LanguageType::C) && cppcheck.analyzes(LanguageType::CPP) &&
                          !cppcheck.analyzes(LanguageType::Python),
                      "the C/C++ tools do not analyze Python");
    }

    // coretrace-python-analyzer checks a whole project: SARIF on stdout, then the user's
    // arguments, then the project root. It runs once for all the Python inputs.
    {
        ctrace::ProgramConfig config = configWithSarif(false);
        const auto args = PythonAnalyzerToolImplementation::buildArguments(config, "proj");
        report.expect((args == std::vector<std::string>{"--check", "--format", "sarif", "proj"}),
                      "coretrace-python-analyzer: --check --format sarif <project root>");
        config.tools.args["coretrace-python-analyzer"] = {"--no-bundled-plugins"};
        const auto extra = PythonAnalyzerToolImplementation::buildArguments(config, "proj");
        report.expect(contains(extra, "--no-bundled-plugins") && extra.back() == "proj",
                      "coretrace-python-analyzer: pass-through arguments precede the root");
        report.expect(PythonAnalyzerToolImplementation().supportsBatchExecution(),
                      "coretrace-python-analyzer: one run for all the Python inputs");
    }

    // coretrace-runtime-analyzer builds and runs one program: SARIF on stdout, the per-program
    // timeout, the binary in ctrace's private run directory, then the user's arguments, and
    // after `--` the build context and the source.
    {
        ctrace::ProgramConfig config = configWithSarif(false);
        const auto args =
            RuntimeAnalyzerToolImplementation::buildArguments(config, "a.c", "/run/program");
        report.expect((args == std::vector<std::string>{"--format", "sarif", "--timeout", "60",
                                                        "-o", "/run/program", "--", "a.c"}),
                      "coretrace-runtime-analyzer: --format sarif --timeout 60 -o <run> -- a.c");

        config.tools.runtime_analyzer_timeout_s = 5;
        config.tools.args["coretrace-runtime-analyzer"] = {"--show-output"};
        config.stack_analyzer.include_dirs = {"inc"};
        config.stack_analyzer.defines = {"FOO=1"};
        const auto derived =
            RuntimeAnalyzerToolImplementation::buildArguments(config, "a.c", "/run/program");
        report.expect((derived == std::vector<std::string>{"--format", "sarif", "--timeout", "5",
                                                           "-o", "/run/program", "--show-output",
                                                           "--", "-Iinc", "-DFOO=1", "a.c"}),
                      "coretrace-runtime-analyzer: timeout, user arguments, then -I/-D and the "
                      "source after --");

        // A bare -I would take the next argument as its directory.
        config.stack_analyzer.include_dirs = {"", "inc"};
        config.stack_analyzer.defines = {"FOO=1", ""};
        report.expect(
            (RuntimeAnalyzerToolImplementation::buildArguments(config, "a.c", "/run/program") ==
             std::vector<std::string>{"--format", "sarif", "--timeout", "5", "-o", "/run/program",
                                      "--show-output", "--", "-Iinc", "-DFOO=1", "a.c"}),
            "coretrace-runtime-analyzer: an empty include directory or define is not passed");

        const RuntimeAnalyzerToolImplementation runtime;
        report.expect(runtime.name() == "coretrace-runtime-analyzer" &&
                          runtime.analyzes(ctrace_defs::LanguageType::C) &&
                          runtime.analyzes(ctrace_defs::LanguageType::CPP) &&
                          !runtime.analyzes(ctrace_defs::LanguageType::Python) &&
                          !runtime.supportsBatchExecution(),
                      "coretrace-runtime-analyzer: one C or C++ program per run");
    }

    // The project root is what the analyzer names modules from (app/helpers.py is app.helpers):
    // the nearest directory holding a project marker, else the file's own directory.
    {
        namespace fs = std::filesystem;
        const fs::path base = fs::temp_directory_path() / "ctrace-python-project-root";
        std::error_code err;
        fs::remove_all(base, err);
        fs::create_directories(base / "proj/app/sub", err);
        fs::create_directories(base / "loose", err);
        std::ofstream(base / "proj/pyproject.toml") << "[project]\n";
        const fs::path proj = fs::canonical(base / "proj");
        const fs::path loose = fs::canonical(base / "loose");

        report.expect(
            PythonAnalyzerToolImplementation::projectRoot((proj / "app/sub/m.py").string()) == proj,
            "project root: the nearest directory with pyproject.toml");
        fs::create_directories(base / "gitrepo/.git", err);
        fs::create_directories(base / "gitrepo/src", err);
        const fs::path gitrepo = fs::canonical(base / "gitrepo");
        report.expect(PythonAnalyzerToolImplementation::projectRoot(
                          (gitrepo / "src/m.py").string()) == gitrepo,
                      "project root: a repository root counts as a project root");
        report.expect(PythonAnalyzerToolImplementation::projectRoot((loose / "m.py").string()) ==
                          loose,
                      "project root: without a marker, the file's own directory");
    }

    // Tools are named, not located: PATH resolves them unless the configuration says otherwise.
    {
        ctrace::ProgramConfig config;
        report.expect(toolCommand(config, CppCheckToolImplementation()) == "cppcheck" &&
                          toolCommand(config, IkosToolImplementation()) == "ikos" &&
                          toolCommand(config, FlawfinderToolImplementation()) == "flawfinder" &&
                          toolCommand(config, TscancodeToolImplementation()) == "tscancode",
                      "default command is the tool name, resolved through PATH");
        report.expect(toolCommand(config, CppCheckToolImplementation()).find('/') ==
                          std::string::npos,
                      "default command carries no path separator");

        config.tools.paths["cppcheck"] = "/opt/homebrew/bin/cppcheck";
        report.expect(toolCommand(config, CppCheckToolImplementation()) ==
                          "/opt/homebrew/bin/cppcheck",
                      "a configured path overrides the lookup");
        report.expect(toolCommand(config, IkosToolImplementation()) == "ikos",
                      "an override for one tool leaves the others alone");
    }
    {
        const auto args =
            TscancodeToolImplementation::buildArguments(configWithSarif(false), "a.c");
        report.expect(contains(args, "--enable=all") && args.back() == "a.c",
                      "tscancode: enables all checks and passes the file");
    }

    // A compilation database, reduced to what replays each unit's compilation elsewhere: the
    // source resolved against its directory, and the arguments that say how it is interpreted,
    // without the compiler, the source itself, output and dependency-file selection, and
    // optimization levels. CMake's "command" strings are split as a shell would.
    {
        const std::filesystem::path dir =
            std::filesystem::temp_directory_path() / "ctrace-compile-commands-tests";
        std::filesystem::create_directories(dir);
        const std::filesystem::path database = dir / "compile_commands.json";
        {
            std::ofstream out(database);
            out << R"json([
  {"directory": "/proj", "file": "src/a.c",
   "arguments": ["/usr/bin/cc", "-c", "-O2", "-I/inc", "-DX=1", "-MD", "-MF", "a.d", "-o", "a.o", "src/a.c"]},
  {"directory": "/proj", "file": "/proj/b.c",
   "command": "cc -c -DMSG=\"hello world\" -I\"/inc dir\" -std=gnu11 -flto -Os -ob.o /proj/b.c"},
  {"directory": "/proj", "file": "src/a.c", "arguments": ["cc", "-DSECOND", "src/a.c"]}
])json";
        }
        std::string error;
        const auto commands = readCompileCommands(database, error);
        report.expect(commands.has_value() && error.empty() && commands->size() == 2,
                      "compile commands: one entry per source, the first when listed twice");
        if (commands.has_value() && commands->size() == 2)
        {
            report.expect((*commands)[0].file == "/proj/src/a.c" &&
                              (*commands)[0].arguments ==
                                  std::vector<std::string>{"-I/inc", "-DX=1"},
                          "compile commands: the source is resolved against its directory; the "
                          "compiler, -c, -O, dependency and output options and the source are "
                          "left out");
            report.expect(
                (*commands)[1].file == "/proj/b.c" &&
                    (*commands)[1].arguments ==
                        std::vector<std::string>{"-DMSG=hello world", "-I/inc dir", "-std=gnu11"},
                "compile commands: a command string is split as a shell would; -flto "
                "and -Os are left out");
        }
        {
            std::ofstream out(database);
            out << R"json({"file": "a.c"})json";
        }
        report.expect(!readCompileCommands(database, error).has_value() && !error.empty(),
                      "compile commands: a document that is not an array of entries is an error");
        report.expect(!readCompileCommands(dir / "missing.json", error).has_value() &&
                          !error.empty(),
                      "compile commands: a database that cannot be read is an error");

        ProgramConfig config;
        report.expect(compileCommandsPath(config).empty(),
                      "compile commands: nothing configured names no database");
        config.files.compile_commands = dir.string();
        report.expect(compileCommandsPath(config) == database,
                      "compile commands: a directory names the database it holds");
        config.files.compile_commands = database.string();
        report.expect(compileCommandsPath(config) == database,
                      "compile commands: a file is taken as is");
        config.files.compile_commands.clear();
        config.files.input = {"main.c", "build/compile_commands.json"};
        report.expect(compileCommandsPath(config) == "build/compile_commands.json",
                      "compile commands: a .json input is a database");
    }

    if (report.failures == 0)
    {
        std::cout << "tool_arguments_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " tool argument check(s) failed\n";
    return 1;
}
