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
        assert(cfg.global.hasStaticAnalysis);
        assert(!cfg.global.hasDynamicAnalysis);
        assert(cfg.global.hasInvokedSpecificTools);
        assert(cfg.global.specificTools.size() == 1);
        assert(cfg.global.specificTools.front() == "ctrace_stack_analyzer");
        assert(cfg.global.hasSarifFormat);
        assert(cfg.global.report_file == "cfg-report.txt");
        assert(cfg.global.output_file == "cfg-output.txt");
        assert(cfg.global.quiet);
        assert(cfg.global.demangle);
        assert(cfg.global.entry_points == "main,helper");
        assert(cfg.global.include_compdb_deps);
        assert(cfg.global.stack_analyzer_mode == "ir");
        assert(cfg.global.stack_analyzer_output_format == "json");
        assert(cfg.global.timing);
        assert(cfg.global.analysis_profile == "full");
        assert(cfg.global.smt == "on");
        assert(cfg.global.smt_timeout_ms == 80U);
        assert(cfg.global.smt_budget_nodes == 1024U);
        assert(cfg.global.stack_limit == 4096U);
        assert(cfg.global.stack_analyzer_extra_args.size() == 2);
        assert(!cfg.files.empty());
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
        assert(cfg.global.analysis_profile == "full");
        assert(cfg.global.smt_timeout_ms == 90U);
        assert(cfg.global.entry_points == "main");
        assert(cfg.global.hasInvokedSpecificTools);
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

        const ctrace::ProgramConfig cfg =
            ctrace::buildConfig(static_cast<int>(argv.size()), argv.data());
        assert(cfg.global.verbose);
        assert(cfg.global.report_file == "from-cli.txt");
        assert(cfg.global.output_file == "from-cli.out");
    }
} // namespace

int main()
{
    testCanonicalConfigParsing();
    testRejectsUnknownRootKey();
    testRejectsInvalidStackAnalyzerMode();
    testLegacyConfigCompatibility();
    testCliOverridesConfig();
    std::cout << "config_parser_tests: all checks passed" << std::endl;
    return 0;
}
