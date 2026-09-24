// SPDX-License-Identifier: Apache-2.0
//
// Each tool reports its own identity and builds its command line from the configuration.
#include "Process/Tools/AnalysisTools.hpp"
#include "ctrace_tools/languageType.hpp"

#include <algorithm>
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

    // coretrace-python-analyzer: SARIF on stdout, then the user's arguments, then the file.
    {
        ctrace::ProgramConfig config = configWithSarif(false);
        const auto args = PythonAnalyzerToolImplementation::buildArguments(config, "app.py");
        report.expect((args == std::vector<std::string>{"--check", "--format", "sarif", "app.py"}),
                      "coretrace-python-analyzer: --check --format sarif <file>");
        config.tools.args["coretrace-python-analyzer"] = {"--no-bundled-plugins"};
        const auto extra = PythonAnalyzerToolImplementation::buildArguments(config, "app.py");
        report.expect(contains(extra, "--no-bundled-plugins") && extra.back() == "app.py",
                      "coretrace-python-analyzer: pass-through arguments precede the file");
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

    if (report.failures == 0)
    {
        std::cout << "tool_arguments_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " tool argument check(s) failed\n";
    return 1;
}
