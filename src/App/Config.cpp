// SPDX-License-Identifier: Apache-2.0
#include "App/Config.hpp"
#include "App/SupportedTools.hpp"
#include "App/ToolConfig.hpp"
#include "App/Version.hpp"
#include "ctrace_defs/types.hpp"
#include "ctrace_tools/strings.hpp"

#include "CLI11.hpp"
#include <coretrace/logger.hpp>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace ctrace
{
    namespace
    {
        constexpr const char* kDescription =
            "ctrace - Static & Dynamic C/C++ Code Analysis Tool. "
            "A modular C/C++ code analysis tool that combines static and dynamic analysis, "
            "configurable to detect vulnerabilities, security issues and memory misuse.";

        constexpr const char* kFooter =
            "Examples:\n"
            "  ctrace --input main.cpp,util.cpp --static --invoke=cppcheck,flawfinder\n"
            "  ctrace --verbose --report-file=analysis.txt --sarif-format";

        /// A rejected command-line value; carries the exact text for stderr.
        struct ConfigError : std::runtime_error
        {
            using std::runtime_error::runtime_error;
        };

        struct OptionSpec
        {
            const char* name;
            const char* valueName; ///< nullptr for flags.
            const char* description;
        };

        // The option list is the CLI contract; `ctrace --help` is generated from it.
        constexpr OptionSpec kOptions[] = {
            {"--verbose,-v", nullptr, "Enables detailed (verbose) output."},
            {"--quiet,-q", nullptr, "Suppresses non-essential output."},
            {"--sarif-format", nullptr, "Generates a report in SARIF format."},
            {"--report-file", "PATH",
             "Specifies the path to the report file (default: ctrace-report.txt)."},
            {"--output-file", "PATH",
             "Specifies the output file for the analysed binary (default: ctrace.out)."},
            {"--entry-points", "NAMES",
             "Sets the entry points for analysis (default: main). Comma-separated list."},
            {"--config", "PATH", "Loads settings from a JSON config file."},
            {"--compile-commands", "PATH",
             "Path to compile_commands.json for tools that support it."},
            {"--include-compdb-deps", nullptr,
             "Includes dependency entries (e.g. _deps) when auto-loading files from "
             "compile_commands.json."},
            {"--analysis-profile", "PROFILE", "Stack analyzer profile: fast|full."},
            {"--smt", "on|off", "Enables/disables SMT refinement in stack analyzer."},
            {"--smt-backend", "NAME", "Primary SMT backend (e.g. z3, interval)."},
            {"--smt-secondary-backend", "NAME", "Secondary backend for multi-solver modes."},
            {"--smt-mode", "MODE", "SMT mode: single|portfolio|cross-check|dual-consensus."},
            {"--smt-timeout-ms", "N", "SMT timeout in milliseconds."},
            {"--smt-budget-nodes", "N", "SMT node budget per query."},
            {"--smt-rules", "LIST", "Comma-separated SMT-enabled rules."},
            {"--resource-model", "PATH", "Path to the resource lifetime model for stack analyzer."},
            {"--escape-model", "PATH", "Path to the stack escape model for stack analyzer."},
            {"--buffer-model", "PATH", "Path to the buffer overflow model for stack analyzer."},
            {"--stack-limit", "BYTES",
             "Stack limit forwarded to the stack analyzer (default: 8388608)."},
            {"--timing", nullptr, "Enables stack analyzer timing output."},
            {"--demangle", nullptr, "Displays demangled function names in supported tools."},
            {"--static", nullptr, "Enables static analysis."},
            {"--dyn", nullptr, "Enables dynamic analysis."},
            {"--invoke", "TOOLS",
             "Invokes specific tools (comma-separated). Available tools: flawfinder, ikos, "
             "cppcheck, tscancode, ctrace_stack_analyzer."},
            {"--input", "FILES", "Specifies the source files to analyse (comma-separated)."},
            {"--ipc", "METHOD", "Specifies the IPC method to use: standardIO|socket|serve."},
            {"--ipc-path", "PATH", "Specifies the IPC path (default: /tmp/coretrace_ipc)."},
            {"--serve-host", "HOST", "HTTP server host when --ipc=serve."},
            {"--serve-port", "PORT", "HTTP server port when --ipc=serve."},
            {"--shutdown-token", "TOKEN", "Token required for POST /shutdown (server mode)."},
            {"--shutdown-timeout-ms", "MS",
             "Graceful shutdown timeout in ms (0 = wait indefinitely)."},
            {"--async", nullptr, "Enables asynchronous execution."},
        };

        void registerOptions(CLI::App& app)
        {
            for (const OptionSpec& spec : kOptions)
            {
                if (spec.valueName == nullptr)
                {
                    app.add_flag(spec.name, spec.description);
                    continue;
                }
                app.add_option(spec.name, spec.description)->type_name(spec.valueName);
            }
        }

        [[nodiscard]] bool wasGiven(const CLI::App& app, const char* option)
        {
            const CLI::Option* parsed = app.get_option_no_throw(option);
            return parsed != nullptr && parsed->count() > 0;
        }

        [[nodiscard]] std::string valueOf(const CLI::App& app, const char* option)
        {
            const CLI::Option* parsed = app.get_option_no_throw(option);
            if (parsed == nullptr || parsed->results().empty())
            {
                return {};
            }
            return parsed->results().back();
        }

        /**
         * Applies the command-line values to the configuration, one handler per option.
         * The config file has already been applied when this runs, so CLI values win
         * (precedence documented in docs/configuration.md).
         */
        class ConfigProcessor
        {
          public:
            explicit ConfigProcessor(ProgramConfig& cfg) : config(cfg)
            {
                commands["--verbose"] = [this](const std::string&)
                { config.output.verbose = true; };
                commands["--quiet"] = [this](const std::string&) { config.output.quiet = true; };
                commands["--demangle"] = [this](const std::string&)
                { config.output.demangle = true; };
                commands["--sarif-format"] = [this](const std::string&)
                { config.output.sarif_format = true; };
                commands["--report-file"] = [this](const std::string& value)
                { config.output.report_file = value; };
                commands["--output-file"] = [this](const std::string& value)
                { config.output.output_file = value; };
                commands["--async"] = [this](const std::string&)
                {
                    config.runtime.async = true;
                    std::cout << "Asynchronous execution enabled." << std::endl;
                };
                commands["--invoke"] = [this](const std::string& value)
                {
                    std::vector<std::string> parts;
                    for (const auto part : ctrace_tools::strings::splitByComma(value))
                    {
                        parts.emplace_back(part);
                    }
                    std::string normalizeError;
                    const auto normalized = normalizeAndValidateToolList(parts, normalizeError);
                    if (!normalizeError.empty())
                    {
                        throw ConfigError(normalizeError + "\n");
                    }
                    config.analysis.invoke = normalized;
                };
                commands["--input"] = [this](const std::string& value) { config.addFile(value); };
                commands["--static"] = [this](const std::string&)
                { config.analysis.static_enabled = true; };
                commands["--dyn"] = [this](const std::string&)
                { config.analysis.dynamic_enabled = true; };
                commands["--entry-points"] = [this](const std::string& value)
                {
                    config.files.entry_points.clear();
                    for (const auto point : ctrace_tools::strings::splitByComma(value))
                    {
                        config.files.entry_points.emplace_back(point);
                    }
                };
                commands["--config"] = [this](const std::string& value)
                { config.config_file = value; };
                commands["--compile-commands"] = [this](const std::string& value)
                { config.files.compile_commands = value; };
                commands["--include-compdb-deps"] = [this](const std::string&)
                { config.files.include_compdb_deps = true; };
                commands["--analysis-profile"] = [this](const std::string& value)
                { config.stack_analyzer.analysis_profile = value; };
                commands["--smt"] = [this](const std::string& value)
                { config.stack_analyzer.smt = value; };
                commands["--smt-backend"] = [this](const std::string& value)
                { config.stack_analyzer.smt_backend = value; };
                commands["--smt-secondary-backend"] = [this](const std::string& value)
                { config.stack_analyzer.smt_secondary_backend = value; };
                commands["--smt-mode"] = [this](const std::string& value)
                { config.stack_analyzer.smt_mode = value; };
                commands["--smt-timeout-ms"] = [this](const std::string& value)
                {
                    try
                    {
                        config.stack_analyzer.smt_timeout_ms =
                            static_cast<uint32_t>(std::stoul(value));
                    }
                    catch (const std::exception& e)
                    {
                        throw ConfigError("Invalid smt timeout value: '" + value +
                                          "'. Error: " + e.what() + "\n");
                    }
                };
                commands["--smt-budget-nodes"] = [this](const std::string& value)
                {
                    try
                    {
                        config.stack_analyzer.smt_budget_nodes = std::stoull(value);
                    }
                    catch (const std::exception& e)
                    {
                        throw ConfigError("Invalid smt budget value: '" + value +
                                          "'. Error: " + e.what() + "\n");
                    }
                };
                commands["--smt-rules"] = [this](const std::string& value)
                {
                    config.stack_analyzer.smt_rules.clear();
                    for (const auto rule : ctrace_tools::strings::splitByComma(value))
                    {
                        config.stack_analyzer.smt_rules.emplace_back(rule);
                    }
                };
                commands["--resource-model"] = [this](const std::string& value)
                { config.stack_analyzer.resource_model = value; };
                commands["--escape-model"] = [this](const std::string& value)
                { config.stack_analyzer.escape_model = value; };
                commands["--buffer-model"] = [this](const std::string& value)
                { config.stack_analyzer.buffer_model = value; };
                commands["--timing"] = [this](const std::string&)
                { config.stack_analyzer.timing = true; };
                commands["--stack-limit"] = [this](const std::string& value)
                {
                    try
                    {
                        config.stack_analyzer.stack_limit = std::stoul(value);
                        coretrace::log(coretrace::Level::Info, "Stack limit set to {} bytes",
                                       config.stack_analyzer.stack_limit);
                    }
                    catch (const std::exception& e)
                    {
                        throw ConfigError("Invalid stack limit value: '" + value +
                                          "'. Error: " + e.what() +
                                          "\nPlease provide a valid unsigned integer.\n");
                    }
                };
                commands["--ipc"] = [this](const std::string& value)
                {
                    const auto& ipc_list = ctrace_defs::IPC_TYPES;
                    if (std::find(ipc_list.begin(), ipc_list.end(), value) == ipc_list.end())
                    {
                        std::string message =
                            "Invalid IPC type: '" + value + "'\n" + "Available IPC types: [";
                        for (const auto& ipc : ipc_list)
                        {
                            message += ipc;
                            if (ipc != ipc_list.back())
                                message += ", ";
                        }
                        message += "]\n";
                        throw ConfigError(message);
                    }
                    config.runtime.ipc = value;
                };
                commands["--ipc-path"] = [this](const std::string& value)
                { config.runtime.ipc_path = value; };
                commands["--serve-host"] = [this](const std::string& value)
                {
                    config.server.host = value;
                    coretrace::log(coretrace::Level::Debug, "Server host set to {}",
                                   config.server.host);
                };
                commands["--serve-port"] = [this](const std::string& value)
                {
                    config.server.port = std::stoi(value);
                    coretrace::log(coretrace::Level::Debug, "Server port set to {}",
                                   config.server.port);
                };
                commands["--shutdown-token"] = [this](const std::string& value)
                { config.server.shutdown_token = value; };
                commands["--shutdown-timeout-ms"] = [this](const std::string& value)
                {
                    config.server.shutdown_timeout_ms = std::stoi(value);
                    if (config.server.shutdown_timeout_ms < 0)
                    {
                        config.server.shutdown_timeout_ms = 0;
                    }
                };
            }

            /// Applies every option that was given; throws ConfigError on the first rejected value.
            void apply(const CLI::App& app)
            {
                for (const auto& [option, command] : commands)
                {
                    if (!wasGiven(app, option.c_str()))
                    {
                        continue;
                    }
                    try
                    {
                        command(valueOf(app, option.c_str()));
                    }
                    catch (const ConfigError&)
                    {
                        throw;
                    }
                    catch (const std::exception& e)
                    {
                        throw ConfigError("Invalid value for " + option + ": " + e.what() + "\n");
                    }
                }
            }

          private:
            ProgramConfig& config;
            std::unordered_map<std::string, std::function<void(const std::string&)>> commands;
        };
    } // namespace

    CT_NODISCARD ConfigResult buildConfig(int argc, char* argv[])
    {
        CLI::App app{kDescription, "ctrace"};
        app.footer(kFooter);
        app.set_help_flag("--help,-h", "Displays this help message.");
        app.set_version_flag("--version,-V", "ctrace " + std::string(build::version()));
        app.get_formatter()->column_width(30);
        registerOptions(app);

        ConfigResult result;
        if (argc <= 1)
        {
            result.output = app.help();
            return result;
        }

        try
        {
            app.parse(argc, argv);
        }
        catch (const CLI::CallForHelp&)
        {
            result.output = app.help();
            return result;
        }
        catch (const CLI::CallForVersion&)
        {
            result.output = app.version() + "\n";
            return result;
        }
        catch (const CLI::ParseError& e)
        {
            result.error = CLI::FailureMessage::simple(&app, e);
            result.exitCode = EXIT_FAILURE;
            return result;
        }

        ProgramConfig config;
        if (wasGiven(app, "--config"))
        {
            const std::string configPath = valueOf(app, "--config");
            std::string toolConfigError;
            if (!applyToolConfigFile(config, configPath, toolConfigError))
            {
                result.error =
                    "Error: failed to load config '" + configPath + "': " + toolConfigError + "\n";
                result.exitCode = EXIT_FAILURE;
                return result;
            }
        }

        try
        {
            ConfigProcessor processor(config);
            processor.apply(app);
        }
        catch (const ConfigError& e)
        {
            result.error = e.what();
            result.exitCode = EXIT_FAILURE;
            return result;
        }

        if (valueOf(app, "--ipc") != "serve" &&
            (wasGiven(app, "--serve-host") || wasGiven(app, "--serve-port")))
        {
            result.output = "[INFO] UNCONSISTENT SERVER OPTIONS: --serve-host or --serve-port "
                            "needed --ipc=serve.\n";
            result.exitCode = 1;
            return result;
        }

        result.config = std::move(config);
        return result;
    }
} // namespace ctrace
