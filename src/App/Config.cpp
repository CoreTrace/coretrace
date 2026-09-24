// SPDX-License-Identifier: Apache-2.0
#include "App/Config.hpp"
#include "App/ShippedDefaults.hpp"
#include "App/ToolConfig.hpp"
#include "App/Version.hpp"
#include "ctrace_defs/types.hpp"
#include "ctrace_tools/strings.hpp"

#include "CLI11.hpp"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

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

        /// How a command-line value becomes a JSON value of the config schema.
        enum class Kind
        {
            Flag,     ///< Presence means true.
            Text,     ///< Kept as a string.
            Unsigned, ///< Must parse as an unsigned integer.
            List      ///< Comma-separated string, split into an array.
        };

        /// One command-line option and where its value lands in the sectioned config
        /// document (docs/configuration.md). The help text is generated from this table.
        struct OptionSpec
        {
            const char* name;      ///< CLI11 spelling, e.g. "--verbose,-v".
            const char* valueName; ///< nullptr for flags.
            const char* description;
            Kind kind;
            const char* section; ///< nullptr for options the front end handles itself.
            const char* key;
        };

        constexpr OptionSpec kOptions[] = {
            {"--verbose,-v", nullptr, "Enables detailed (verbose) output.", Kind::Flag, "output",
             "verbose"},
            {"--quiet,-q", nullptr, "Suppresses non-essential output.", Kind::Flag, "output",
             "quiet"},
            {"--sarif-format", nullptr, "Generates a report in SARIF format.", Kind::Flag, "output",
             "sarif_format"},
            {"--report-file", "PATH",
             "Specifies the path to the report file (default: ctrace-report.txt).", Kind::Text,
             "output", "report_file"},
            {"--output-file", "PATH",
             "Specifies the output file for the analysed binary (default: ctrace.out).", Kind::Text,
             "output", "output_file"},
            {"--entry-points", "NAMES",
             "Sets the entry points for analysis (default: main). Comma-separated list.",
             Kind::List, "files", "entry_points"},
            {"--config", "PATH", "Loads settings from a JSON config file.", Kind::Text, nullptr,
             nullptr},
            {"--compile-commands", "PATH",
             "Path to compile_commands.json for tools that support it.", Kind::Text, "files",
             "compile_commands"},
            {"--include-compdb-deps", nullptr,
             "Includes dependency entries (e.g. _deps) when auto-loading files from "
             "compile_commands.json.",
             Kind::Flag, "files", "include_compdb_deps"},
            {"--analysis-profile", "PROFILE", "Stack analyzer profile: fast|full.", Kind::Text,
             "stack_analyzer", "analysis_profile"},
            {"--smt", "on|off", "Enables/disables SMT refinement in stack analyzer.", Kind::Text,
             "stack_analyzer", "smt"},
            {"--smt-backend", "NAME", "Primary SMT backend (e.g. z3, interval).", Kind::Text,
             "stack_analyzer", "smt_backend"},
            {"--smt-secondary-backend", "NAME", "Secondary backend for multi-solver modes.",
             Kind::Text, "stack_analyzer", "smt_secondary_backend"},
            {"--smt-mode", "MODE", "SMT mode: single|portfolio|cross-check|dual-consensus.",
             Kind::Text, "stack_analyzer", "smt_mode"},
            {"--smt-timeout-ms", "N", "SMT timeout in milliseconds.", Kind::Unsigned,
             "stack_analyzer", "smt_timeout_ms"},
            {"--smt-budget-nodes", "N", "SMT node budget per query.", Kind::Unsigned,
             "stack_analyzer", "smt_budget_nodes"},
            {"--smt-rules", "LIST", "Comma-separated SMT-enabled rules.", Kind::List,
             "stack_analyzer", "smt_rules"},
            {"--resource-model", "PATH", "Path to the resource lifetime model for stack analyzer.",
             Kind::Text, "stack_analyzer", "resource_model"},
            {"--escape-model", "PATH", "Path to the stack escape model for stack analyzer.",
             Kind::Text, "stack_analyzer", "escape_model"},
            {"--buffer-model", "PATH", "Path to the buffer overflow model for stack analyzer.",
             Kind::Text, "stack_analyzer", "buffer_model"},
            {"--stack-limit", "BYTES",
             "Stack limit forwarded to the stack analyzer (default: 8388608).", Kind::Unsigned,
             "stack_analyzer", "stack_limit"},
            {"--timing", nullptr, "Enables stack analyzer timing output.", Kind::Flag,
             "stack_analyzer", "timing"},
            {"--demangle", nullptr, "Displays demangled function names in supported tools.",
             Kind::Flag, "output", "demangle"},
            {"--static", nullptr, "Enables static analysis.", Kind::Flag, "analysis", "static"},
            {"--dyn", nullptr, "Enables dynamic analysis.", Kind::Flag, "analysis", "dynamic"},
            {"--invoke", "TOOLS",
             "Invokes specific tools (comma-separated). Available tools: flawfinder, ikos, "
             "cppcheck, tscancode, ctrace_stack_analyzer.",
             Kind::List, "analysis", "invoke"},
            {"--input", "FILES", "Specifies the source files to analyse (comma-separated).",
             Kind::List, "files", "input"},
            {"--fail-on", "LEVEL",
             "Exit code 2 when a finding at or above LEVEL is reported: error|warning|none "
             "(default: error). Exit code 3 when a tool could not run.",
             Kind::Text, "analysis", "fail_on"},
            {"--ipc", "METHOD", "Specifies the IPC method to use: standardIO|socket|serve.",
             Kind::Text, "runtime", "ipc"},
            {"--ipc-path", "PATH", "Specifies the IPC path (default: /tmp/coretrace_ipc).",
             Kind::Text, "runtime", "ipc_path"},
            {"--serve-host", "HOST", "HTTP server host when --ipc=serve.", Kind::Text, "server",
             "host"},
            {"--serve-port", "PORT", "HTTP server port when --ipc=serve.", Kind::Unsigned, "server",
             "port"},
            {"--shutdown-token", "TOKEN", "Token required for POST /shutdown (server mode).",
             Kind::Text, "server", "shutdown_token"},
            {"--shutdown-timeout-ms", "MS",
             "Graceful shutdown timeout in ms (0 = wait indefinitely).", Kind::Unsigned, "server",
             "shutdown_timeout_ms"},
            {"--async", nullptr, "Enables asynchronous execution.", Kind::Flag, "runtime", "async"},
        };

        /// "--report-file,-r" -> "--report-file".
        [[nodiscard]] std::string longName(const OptionSpec& spec)
        {
            const std::string name(spec.name);
            return name.substr(0, name.find(','));
        }

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

        /// Builds the sectioned config document from the options that were given.
        /// Returns false with `error` set when a value cannot be converted.
        [[nodiscard]] bool documentFromOptions(const CLI::App& app, nlohmann::json& document,
                                               std::string& error)
        {
            document = nlohmann::json::object();
            for (const OptionSpec& spec : kOptions)
            {
                if (spec.section == nullptr)
                {
                    continue;
                }
                const std::string option = longName(spec);
                if (!wasGiven(app, option.c_str()))
                {
                    continue;
                }
                const std::string value = valueOf(app, option.c_str());
                nlohmann::json converted;
                switch (spec.kind)
                {
                case Kind::Flag:
                    converted = true;
                    break;
                case Kind::Text:
                    converted = value;
                    break;
                case Kind::Unsigned:
                {
                    const bool digitsOnly =
                        !value.empty() &&
                        std::all_of(value.begin(), value.end(),
                                    [](unsigned char ch) { return std::isdigit(ch) != 0; });
                    try
                    {
                        if (!digitsOnly)
                        {
                            throw std::out_of_range("not a number");
                        }
                        converted = static_cast<std::uint64_t>(std::stoull(value));
                    }
                    catch (const std::exception&)
                    {
                        error = "Invalid value for " + option + ": '" + value +
                                "' is not an unsigned integer.\n";
                        return false;
                    }
                    break;
                }
                case Kind::List:
                {
                    nlohmann::json items = nlohmann::json::array();
                    for (const auto item : ctrace_tools::strings::splitByComma(value))
                    {
                        items.push_back(std::string(item));
                    }
                    converted = std::move(items);
                    break;
                }
                }
                document[spec.section][spec.key] = std::move(converted);
            }
            return true;
        }

        /// Loader diagnostics name the sectioned key; the user typed the option.
        [[nodiscard]] std::string withOptionNames(std::string message)
        {
            std::vector<std::pair<std::string, std::string>> renames;
            for (const OptionSpec& spec : kOptions)
            {
                if (spec.section != nullptr)
                {
                    renames.emplace_back(std::string(spec.section) + "." + spec.key,
                                         longName(spec));
                }
            }
            std::sort(renames.begin(), renames.end(),
                      [](const auto& a, const auto& b) { return a.first.size() > b.first.size(); });
            for (const auto& [path, option] : renames)
            {
                for (auto pos = message.find(path); pos != std::string::npos;
                     pos = message.find(path, pos + option.size()))
                {
                    message.replace(pos, path.size(), option);
                }
            }
            return message;
        }

        /// The running executable, from the OS when it knows (installed binaries are usually
        /// started through PATH, so argv[0] alone is not enough), else from argv[0].
        [[nodiscard]] std::filesystem::path executablePath(const char* argv0)
        {
            std::error_code err;
#if defined(__APPLE__)
            std::uint32_t size = 0;
            _NSGetExecutablePath(nullptr, &size); // Reports the required buffer size.
            std::string buffer(size, '\0');
            if (_NSGetExecutablePath(buffer.data(), &size) == 0)
            {
                buffer.resize(std::char_traits<char>::length(buffer.c_str()));
                return std::filesystem::canonical(buffer, err);
            }
#elif defined(__linux__)
            if (const auto self = std::filesystem::read_symlink("/proc/self/exe", err); !err)
            {
                return self;
            }
#endif
            return std::filesystem::absolute(argv0 == nullptr ? "" : argv0, err);
        }

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
            if (!applyToolConfigFile(config, configPath, toolConfigError, &result.warnings))
            {
                result.error =
                    "Error: failed to load config '" + configPath + "': " + toolConfigError + "\n";
                result.exitCode = EXIT_FAILURE;
                return result;
            }
        }

        // Precedence: defaults < config file < command line. CLI values go through the same
        // loader as the file, so validation and normalization are identical.
        nlohmann::json document;
        std::string loaderError;
        if (!documentFromOptions(app, document, loaderError) ||
            !applyToolConfigObject(config, document, {}, loaderError))
        {
            result.error = withOptionNames(loaderError);
            if (result.error.empty() || result.error.back() != '\n')
            {
                result.error += '\n';
            }
            result.exitCode = EXIT_FAILURE;
            return result;
        }
        if (wasGiven(app, "--config"))
        {
            config.config_file = valueOf(app, "--config");
        }
        result.executable = executablePath(argv[0]);
        applyShippedDefaults(config, result.executable, result.warnings);
        if (wasGiven(app, "--async"))
        {
            result.output += "Asynchronous execution enabled.\n";
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
