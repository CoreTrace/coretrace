// SPDX-License-Identifier: Apache-2.0
#include "App/Config.hpp"
#include "App/ToolConfig.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    std::filesystem::path makeTempConfigPath(const std::string& fileName)
    {
        const auto base = std::filesystem::temp_directory_path() / "ctrace-config-tests";
        std::error_code err;
        std::filesystem::create_directories(base, err);
        return base / fileName;
    }

    void writeTextFile(const std::filesystem::path& path, const std::string& content)
    {
        std::ofstream out(path);
        if (!out.is_open())
        {
            std::cerr << "Failed to open file for writing: " << path << std::endl;
            std::exit(1);
        }
        out << content;
    }

    void testCanonicalConfigParsing()
    {
        const auto path = makeTempConfigPath("canonical.json");
        writeTextFile(path, R"json(
{
  "schema_version": 1,
  "analysis": {
    "static": true,
    "dynamic": false,
    "invoke": ["ctrace_stack_analyzer"]
  },
  "files": {
    "input": ["./tests/buffer_overflow.cc"],
    "entry_points": ["main", "helper"],
    "compile_commands": "",
    "include_compdb_deps": true
  },
  "output": {
    "sarif_format": true,
    "report_file": "cfg-report.txt",
    "output_file": "cfg-output.txt",
    "verbose": false,
    "quiet": true,
    "demangle": true
  },
  "runtime": {
    "async": false,
    "ipc": "standardIO",
    "ipc_path": "/tmp/coretrace-test-ipc"
  },
  "server": {
    "host": "127.0.0.1",
    "port": 8081,
    "shutdown_token": "token",
    "shutdown_timeout_ms": 500
  },
  "stack_analyzer": {
    "mode": "ir",
    "output_format": "json",
    "timing": true,
    "analysis_profile": "full",
    "smt": "on",
    "smt_backend": "z3",
    "smt_secondary_backend": "single",
    "smt_mode": "single",
    "smt_timeout_ms": 80,
    "smt_budget_nodes": 1024,
    "smt_rules": ["stack-buffer"],
    "stack_limit": 4096,
    "resource_model": "./resource.txt",
    "escape_model": "./escape.txt",
    "buffer_model": "./buffer.txt",
    "extra_args": ["--foo=bar", "--baz"]
  }
}
)json");

        ctrace::ProgramConfig cfg;
        std::string err;
        const bool ok = ctrace::applyToolConfigFile(cfg, path.string(), err);
        assert(ok);
        assert(err.empty());
        assert(cfg.analysis.static_enabled);
        assert(!cfg.analysis.dynamic_enabled);
        assert(cfg.analysis.invoke.size() == 1);
        assert(cfg.analysis.invoke.front() == "ctrace_stack_analyzer");
        assert(cfg.output.sarif_format);
        assert(cfg.output.report_file == "cfg-report.txt");
        assert(cfg.output.output_file == "cfg-output.txt");
        assert(cfg.output.quiet);
        assert(cfg.output.demangle);
        assert((cfg.files.entry_points == std::vector<std::string>{"main", "helper"}));
        assert(cfg.files.include_compdb_deps);
        assert(cfg.stack_analyzer.mode == "ir");
        assert(cfg.stack_analyzer.output_format == "json");
        assert(cfg.stack_analyzer.timing);
        assert(cfg.stack_analyzer.analysis_profile == "full");
        assert(cfg.stack_analyzer.smt == "on");
        assert(cfg.stack_analyzer.smt_timeout_ms == 80U);
        assert(cfg.stack_analyzer.smt_budget_nodes == 1024U);
        assert(cfg.stack_analyzer.stack_limit == 4096U);
        assert(cfg.stack_analyzer.extra_args.size() == 2);
        assert(!cfg.files.input.empty());
    }

    void testRejectsUnknownRootKey()
    {
        const auto path = makeTempConfigPath("unknown-root.json");
        writeTextFile(path, R"json(
{
  "schema_version": 1,
  "unknown_key": true
}
)json");

        ctrace::ProgramConfig cfg;
        std::string err;
        const bool ok = ctrace::applyToolConfigFile(cfg, path.string(), err);
        assert(!ok);
        assert(err.find("Unknown key 'unknown_key' in 'root'") != std::string::npos);
    }

    void testRejectsInvalidStackAnalyzerMode()
    {
        const auto path = makeTempConfigPath("invalid-smt-mode.json");
        writeTextFile(path, R"json(
{
  "schema_version": 1,
  "stack_analyzer": {
    "smt_mode": "bad-mode"
  }
}
)json");

        ctrace::ProgramConfig cfg;
        std::string err;
        const bool ok = ctrace::applyToolConfigFile(cfg, path.string(), err);
        assert(!ok);
        assert(err.find("smt_mode") != std::string::npos);
    }

    void testLegacyConfigCompatibility()
    {
        const auto path = makeTempConfigPath("legacy.json");
        writeTextFile(path, R"json(
{
  "invoke": ["ctrace_stack_analyzer"],
  "input": ["./tests/buffer_overflow.cc"],
  "stack_analyzer": {
    "analysis-profile": "full",
    "smt-timeout-ms": 90,
    "entry_points": ["main"]
  }
}
)json");

        ctrace::ProgramConfig cfg;
        std::string err;
        const bool ok = ctrace::applyToolConfigFile(cfg, path.string(), err);
        assert(ok);
        assert(err.empty());
        assert(cfg.stack_analyzer.analysis_profile == "full");
        assert(cfg.stack_analyzer.smt_timeout_ms == 90U);
        assert((cfg.files.entry_points == std::vector<std::string>{"main"}));
        assert(!cfg.analysis.invoke.empty());
    }

    void testCliOverridesConfig()
    {
        const auto path = makeTempConfigPath("precedence.json");
        writeTextFile(path, R"json(
{
  "schema_version": 1,
  "output": {
    "verbose": false,
    "report_file": "from-config.txt",
    "output_file": "from-config.out"
  },
  "analysis": {
    "invoke": ["ctrace_stack_analyzer"]
  }
}
)json");

        std::vector<std::string> args = {
            "ctrace",        "--config",     path.string(),   "--verbose",
            "--report-file", "from-cli.txt", "--output-file", "from-cli.out",
        };
        std::vector<char*> argv;
        argv.reserve(args.size());
        for (auto& arg : args)
        {
            argv.push_back(arg.data());
        }

        const ctrace::ConfigResult result =
            ctrace::buildConfig(static_cast<int>(argv.size()), argv.data());
        assert(result.config.has_value());
        const ctrace::ProgramConfig& cfg = *result.config;
        assert(cfg.output.verbose);
        assert(cfg.output.report_file == "from-cli.txt");
        assert(cfg.output.output_file == "from-cli.out");
    }

    ctrace::ConfigResult buildFromArgs(std::vector<std::string> args)
    {
        std::vector<char*> argv;
        argv.reserve(args.size());
        for (auto& arg : args)
        {
            argv.push_back(arg.data());
        }
        return ctrace::buildConfig(static_cast<int>(argv.size()), argv.data());
    }

    // Invalid values must come back as a result, never as a process exit, so that main is the
    // only place that terminates the program.
    void testInvalidIpcIsAnError()
    {
        const auto result = buildFromArgs({"ctrace", "--ipc", "bogus"});
        assert(!result.config.has_value());
        assert(result.exitCode == 1);
        assert(result.error ==
               "Invalid value 'bogus' for '--ipc'. Allowed values: [standardIO, socket, serve]\n");
    }

    void testUnknownToolIsAnError()
    {
        const auto result = buildFromArgs({"ctrace", "--invoke", "nope"});
        assert(!result.config.has_value());
        assert(result.exitCode == 1);
        assert(result.error.rfind("--invoke: Unknown tool 'nope'. Allowed tools: [", 0) == 0);
    }

    void testInvalidSmtTimeoutIsAnError()
    {
        const auto result = buildFromArgs({"ctrace", "--smt-timeout-ms", "abc"});
        assert(!result.config.has_value());
        assert(result.exitCode == 1);
        assert(result.error ==
               "Invalid value for --smt-timeout-ms: 'abc' is not an unsigned integer.\n");
    }

    void testInvalidPortIsAnError()
    {
        const auto result = buildFromArgs({"ctrace", "--ipc", "serve", "--serve-port", "abc"});
        assert(!result.config.has_value());
        assert(result.exitCode == 1);
        assert(result.error ==
               "Invalid value for --serve-port: 'abc' is not an unsigned integer.\n");
    }

    // CLI values get the same validation and normalization as the config file.
    void testCliValuesShareTheLoaderRules()
    {
        const auto smt = buildFromArgs({"ctrace", "--smt", "yes", "--input", "a.c"});
        assert(smt.config.has_value());
        assert(smt.config->stack_analyzer.smt == "on");

        const auto profile = buildFromArgs({"ctrace", "--analysis-profile", "turbo"});
        assert(!profile.config.has_value());
        assert(profile.error ==
               "Invalid value 'turbo' for '--analysis-profile'. Allowed values: [fast, full]\n");

        const auto typed =
            buildFromArgs({"ctrace", "--stack-limit", "4096", "--smt-budget-nodes", "7",
                           "--entry-points", "a,b", "--invoke", "cppcheck,ctrace_stack_analyzer",
                           "--static", "--async", "--ipc", "serve", "--serve-port", "8081"});
        assert(typed.config.has_value());
        const ctrace::ProgramConfig& cfg = *typed.config;
        assert(cfg.stack_analyzer.stack_limit == 4096U);
        assert(cfg.stack_analyzer.smt_budget_nodes == 7U);
        assert((cfg.files.entry_points == std::vector<std::string>{"a", "b"}));
        assert(
            (cfg.analysis.invoke == std::vector<std::string>{"cppcheck", "ctrace_stack_analyzer"}));
        assert(cfg.analysis.static_enabled);
        assert(cfg.runtime.async);
        assert(cfg.runtime.ipc == "serve");
        assert(cfg.server.port == 8081);
    }

    void testMissingConfigFileIsAnError()
    {
        const auto result = buildFromArgs({"ctrace", "--config", "/nonexistent/ctrace.json"});
        assert(!result.config.has_value());
        assert(result.exitCode == 1);
        assert(result.error.find("failed to load config '/nonexistent/ctrace.json'") !=
               std::string::npos);
    }

    void testUnknownOptionIsAnError()
    {
        const auto result = buildFromArgs({"ctrace", "--nope"});
        assert(!result.config.has_value());
        assert(result.exitCode == 1);
        assert(result.error.find("--nope") != std::string::npos);
    }

    void testHelpAndVersionAreReturnedNotPrinted()
    {
        const auto help = buildFromArgs({"ctrace", "--help"});
        assert(!help.config.has_value());
        assert(help.exitCode == 0);
        assert(help.output.find("--stack-limit") != std::string::npos);

        const auto version = buildFromArgs({"ctrace", "--version"});
        assert(!version.config.has_value());
        assert(version.exitCode == 0);
        assert(version.output.rfind("ctrace ", 0) == 0);

        const auto noArgs = buildFromArgs({"ctrace"});
        assert(!noArgs.config.has_value());
        assert(noArgs.exitCode == 0);
        assert(noArgs.output.find("--help") != std::string::npos);
    }
    // Characterization of the loader: every rule below must hold before and after the schema
    // table refactor (ROADMAP M1).
    ctrace::ProgramConfig loadOrDie(const std::string& fileName, const std::string& content)
    {
        const auto path = makeTempConfigPath(fileName);
        writeTextFile(path, content);
        ctrace::ProgramConfig cfg;
        std::string err;
        const bool ok = ctrace::applyToolConfigFile(cfg, path.string(), err);
        if (!ok)
        {
            std::cerr << "unexpected config error for " << fileName << ": " << err << std::endl;
            std::exit(1);
        }
        return cfg;
    }

    std::string loadError(const std::string& fileName, const std::string& content)
    {
        const auto path = makeTempConfigPath(fileName);
        writeTextFile(path, content);
        ctrace::ProgramConfig cfg;
        std::string err;
        const bool ok = ctrace::applyToolConfigFile(cfg, path.string(), err);
        assert(!ok);
        return err;
    }

    void testCanonicalRepoConfigLoads()
    {
        const auto repoConfig =
            std::filesystem::path(__FILE__).parent_path().parent_path() / "config/tool-config.json";
        ctrace::ProgramConfig cfg;
        std::string err;
        const bool ok = ctrace::applyToolConfigFile(cfg, repoConfig.string(), err);
        assert(ok);
        assert((cfg.analysis.invoke == std::vector<std::string>{"ctrace_stack_analyzer"}));
        assert(cfg.output.report_file == "coretrace-results.json");
        assert(cfg.output.demangle);
        assert(cfg.stack_analyzer.smt == "on");
        assert(cfg.stack_analyzer.smt_backend == "z3");
        assert(cfg.stack_analyzer.smt_rules.size() == 6);
        assert(cfg.stack_analyzer.stack_limit == 8388608U);
        assert(cfg.stack_analyzer.resource_cross_tu.has_value() &&
               *cfg.stack_analyzer.resource_cross_tu);
        // Relative model paths are resolved from the config directory.
        assert(std::filesystem::path(cfg.stack_analyzer.resource_model).is_absolute());
        assert(cfg.stack_analyzer.resource_model.find("models/resource-lifetime/generic.txt") !=
               std::string::npos);
        assert(cfg.stack_analyzer.resource_summary_cache_dir.find(".cache/resource-lifetime") !=
               std::string::npos);
        assert(cfg.config_file == repoConfig.lexically_normal().string());
    }

    void testAliasesAndValueForms()
    {
        const auto cfg = loadOrDie("aliases.json", R"json(
{
  "analysis": {"static_analysis": true},
  "stack_analyzer": {
    "smt": "yes",
    "no-resource-cross-tu": true,
    "uninitialized-cross-tu": false,
    "only-func": ["f1", "f2"],
    "format": "sarif",
    "stl": true,
    "jobs": 4,
    "compile-ir-format": ".LL",
    "smt-timeout-ms": 12,
    "define": "X=1",
    "print-effective-config": true
  }
}
)json");
        assert(cfg.analysis.static_enabled);
        assert(cfg.stack_analyzer.smt == "on");
        assert(cfg.stack_analyzer.resource_cross_tu.has_value() &&
               !*cfg.stack_analyzer.resource_cross_tu);
        assert(cfg.stack_analyzer.uninitialized_cross_tu.has_value() &&
               !*cfg.stack_analyzer.uninitialized_cross_tu);
        assert((cfg.stack_analyzer.only_functions == std::vector<std::string>{"f1", "f2"}));
        assert(cfg.stack_analyzer.output_format == "sarif");
        assert(cfg.stack_analyzer.include_stl);
        assert(cfg.stack_analyzer.jobs == "4");
        assert(cfg.stack_analyzer.compile_ir_format == ".LL");
        assert(cfg.stack_analyzer.smt_timeout_ms == 12U);
        assert((cfg.stack_analyzer.defines == std::vector<std::string>{"X=1"}));
        assert(cfg.stack_analyzer.print_effective_config);
    }

    void testStackAnalyzerEntryPointsFeedBothFields()
    {
        const auto cfg = loadOrDie("entry-both.json", R"json(
{"stack_analyzer": {"only_functions": ["old"], "entry_points": ["f"]}}
)json");
        assert((cfg.files.entry_points == std::vector<std::string>{"f"}));
        assert((cfg.stack_analyzer.only_functions == std::vector<std::string>{"f"}));
    }

    void testToolsSectionAliasAndCanonicalPrecedence()
    {
        const auto cfg = loadOrDie("tools-alias.json", R"json(
{
  "tools": {"stack_analyzer": {"mode": "abi", "verbose": true}},
  "output": {"verbose": false}
}
)json");
        assert(cfg.stack_analyzer.mode == "abi");
        // Canonical sections are applied last and win over the analyzer section duplicates.
        assert(!cfg.output.verbose);
    }

    void testPathsAreResolvedFromConfigDirectory()
    {
        const auto cfg = loadOrDie("paths.json", R"json(
{
  "files": {"input": ["./a.c", "b.c"], "compile_commands": "./build"},
  "stack_analyzer": {"escape_model": "", "base_dir": "src"}
}
)json");
        const auto dir = makeTempConfigPath("paths.json").parent_path();
        assert(cfg.files.input.size() == 2);
        assert(cfg.files.input[0] == (dir / "a.c").lexically_normal().string());
        assert(cfg.files.input[1] == (dir / "b.c").lexically_normal().string());
        assert(cfg.files.compile_commands == (dir / "build").lexically_normal().string());
        assert(cfg.stack_analyzer.escape_model.empty());
        assert(cfg.stack_analyzer.base_dir == (dir / "src").lexically_normal().string());
    }

    void testValidationMessages()
    {
        assert(loadError("port.json", R"({"server": {"port": 70000}})") ==
               "server.port must be between 0 and 65535.");
        assert(loadError("ipc.json", R"({"runtime": {"ipc": "bogus"}})")
                   .find("Invalid value 'bogus' for 'runtime.ipc'") != std::string::npos);
        assert(loadError("jobs.json", R"({"stack_analyzer": {"jobs": 0}})")
                   .find("jobs must be >= 1 or 'auto'") != std::string::npos);
        assert(loadError("mode.json", R"({"stack_analyzer": {"mode": "x"}})")
                   .find("Allowed values: [ir, abi]") != std::string::npos);
        assert(loadError("tool.json", R"({"analysis": {"invoke": ["nope"]}})")
                   .find("analysis.invoke: Unknown tool 'nope'") != std::string::npos);
        assert(loadError("unknown-section-key.json", R"({"output": {"colour": true}})")
                   .find("Unknown key 'colour' in 'output'") != std::string::npos);
        assert(loadError("type.json", R"({"output": {"verbose": "yes"}})") ==
               "Expected boolean for 'output.verbose'.");
        assert(loadError("smt-type.json", R"({"stack_analyzer": {"smt": 3}})") ==
               "Expected boolean or bool-like string for 'stack_analyzer.smt'.");
        assert(loadError("timeout.json", R"({"stack_analyzer": {"smt_timeout_ms": 5000000000}})") ==
               "stack_analyzer.smt_timeout_ms is too large.");
        assert(loadError("schema.json", R"({"schema_version": 2})")
                   .find("Unsupported schema_version '2'") != std::string::npos);
        assert(loadError("legacy-input.json", R"({"input": {"a": 1}})") ==
               "Expected string or array of strings for 'input'.");
    }
} // namespace

int main()
{
    testCanonicalConfigParsing();
    testRejectsUnknownRootKey();
    testRejectsInvalidStackAnalyzerMode();
    testLegacyConfigCompatibility();
    testCliOverridesConfig();
    testInvalidIpcIsAnError();
    testUnknownToolIsAnError();
    testInvalidSmtTimeoutIsAnError();
    testInvalidPortIsAnError();
    testCliValuesShareTheLoaderRules();
    testMissingConfigFileIsAnError();
    testUnknownOptionIsAnError();
    testHelpAndVersionAreReturnedNotPrinted();
    testCanonicalRepoConfigLoads();
    testAliasesAndValueForms();
    testStackAnalyzerEntryPointsFeedBothFields();
    testToolsSectionAliasAndCanonicalPrecedence();
    testPathsAreResolvedFromConfigDirectory();
    testValidationMessages();
    std::cout << "config_parser_tests: all checks passed" << std::endl;
    return 0;
}
