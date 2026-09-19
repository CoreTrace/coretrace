// SPDX-License-Identifier: Apache-2.0
#include "App/ToolConfig.hpp"

#include "App/SupportedTools.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace ctrace
{
    namespace
    {
        using json = nlohmann::json;

        constexpr uint64_t kToolConfigSchemaVersion = 1;

        // ------------------------------------------------------------------ helpers

        [[nodiscard]] std::filesystem::path
        resolvePathFromBase(const std::filesystem::path& baseDir, std::string_view rawPath)
        {
            std::filesystem::path path(rawPath);
            if (path.is_relative())
            {
                path = baseDir / path;
            }
            return path.lexically_normal();
        }

        [[nodiscard]] std::string toLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return value;
        }

        [[nodiscard]] std::string trimCopy(std::string_view input)
        {
            std::size_t start = 0;
            while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])))
            {
                ++start;
            }
            std::size_t end = input.size();
            while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])))
            {
                --end;
            }
            return std::string(input.substr(start, end - start));
        }

        [[nodiscard]] std::string joinKeys(const std::vector<const char*>& keys)
        {
            std::string joined;
            for (std::size_t i = 0; i < keys.size(); ++i)
            {
                if (i > 0)
                {
                    joined += ", ";
                }
                joined += keys[i];
            }
            return joined;
        }

        [[nodiscard]] bool validateKnownKeys(const json& object,
                                             const std::vector<const char*>& allowedKeys,
                                             std::string_view context, std::string& errorMessage)
        {
            if (!object.is_object())
            {
                errorMessage = "Expected JSON object for '" + std::string(context) + "'.";
                return false;
            }
            std::unordered_set<std::string> allowed(allowedKeys.begin(), allowedKeys.end());
            for (auto it = object.begin(); it != object.end(); ++it)
            {
                if (allowed.find(it.key()) == allowed.end())
                {
                    errorMessage = "Unknown key '" + it.key() + "' in '" + std::string(context) +
                                   "'. Allowed keys: [" + joinKeys(allowedKeys) + "]";
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool loadJsonFile(const std::filesystem::path& filePath, json& out,
                                        std::string& errorMessage)
        {
            std::ifstream input(filePath);
            if (!input.is_open())
            {
                errorMessage = "Unable to open file: " + filePath.string();
                return false;
            }
            std::ostringstream buffer;
            buffer << input.rdbuf();
            out = json::parse(buffer.str(), nullptr, false);
            if (out.is_discarded())
            {
                errorMessage = "Invalid JSON in file: " + filePath.string();
                return false;
            }
            if (!out.is_object())
            {
                errorMessage = "Config root must be a JSON object.";
                return false;
            }
            return true;
        }

        [[nodiscard]] bool parseStringList(const json& value, std::vector<std::string>& out,
                                           std::string& errorMessage,
                                           const std::string& locationPath)
        {
            out.clear();
            if (value.is_string())
            {
                out.push_back(value.get<std::string>());
                return true;
            }
            if (!value.is_array())
            {
                errorMessage = "Expected string or array of strings for '" + locationPath + "'.";
                return false;
            }
            out.reserve(value.size());
            for (const auto& item : value)
            {
                if (!item.is_string())
                {
                    errorMessage = "Expected only strings in '" + locationPath + "'.";
                    return false;
                }
                out.push_back(item.get<std::string>());
            }
            return true;
        }

        [[nodiscard]] bool parseBoolLike(const json& value, bool& out, std::string& errorMessage,
                                         const std::string& locationPath)
        {
            if (value.is_boolean())
            {
                out = value.get<bool>();
                return true;
            }
            if (value.is_string())
            {
                const auto lowered = toLower(value.get<std::string>());
                if (lowered == "true" || lowered == "on" || lowered == "1" || lowered == "yes")
                {
                    out = true;
                    return true;
                }
                if (lowered == "false" || lowered == "off" || lowered == "0" || lowered == "no")
                {
                    out = false;
                    return true;
                }
            }
            errorMessage = "Expected boolean or bool-like string for '" + locationPath + "'.";
            return false;
        }

        // ---------------------------------------------------------------- validators

        using Validator = bool (*)(const std::string& value, const std::string& locationPath,
                                   std::string& errorMessage);

        [[nodiscard]] bool invalidValue(const std::string& value, const std::string& locationPath,
                                        const std::string& allowed, std::string& errorMessage)
        {
            errorMessage = "Invalid value '" + value + "' for '" + locationPath +
                           "'. Allowed values: [" + allowed + "]";
            return false;
        }

        [[nodiscard]] bool validateIpcValue(const std::string& value,
                                            const std::string& locationPath,
                                            std::string& errorMessage)
        {
            const auto& ipcList = ctrace_defs::IPC_TYPES;
            if (value.empty() || std::find(ipcList.begin(), ipcList.end(), value) != ipcList.end())
            {
                return true;
            }
            std::string allowed;
            for (std::size_t i = 0; i < ipcList.size(); ++i)
            {
                allowed += (i > 0 ? ", " : "") + ipcList[i];
            }
            return invalidValue(value, locationPath, allowed, errorMessage);
        }

        [[nodiscard]] bool validateAnalysisProfile(const std::string& value,
                                                   const std::string& locationPath,
                                                   std::string& errorMessage)
        {
            if (value.empty() || value == "fast" || value == "full")
            {
                return true;
            }
            return invalidValue(value, locationPath, "fast, full", errorMessage);
        }

        [[nodiscard]] bool validateSmtMode(const std::string& value,
                                           const std::string& locationPath,
                                           std::string& errorMessage)
        {
            static const std::unordered_set<std::string> allowed = {
                "single", "portfolio", "cross-check", "dual-consensus"};
            if (value.empty() || allowed.find(value) != allowed.end())
            {
                return true;
            }
            return invalidValue(value, locationPath,
                                "single, portfolio, cross-check, dual-consensus", errorMessage);
        }

        [[nodiscard]] bool validateCompileIRFormat(const std::string& value,
                                                   const std::string& locationPath,
                                                   std::string& errorMessage)
        {
            if (value.empty())
            {
                return true;
            }
            std::string lowered = toLower(value);
            if (!lowered.empty() && lowered.front() == '.')
            {
                lowered.erase(lowered.begin());
            }
            if (lowered == "bc" || lowered == "ll")
            {
                return true;
            }
            return invalidValue(value, locationPath, "bc, ll", errorMessage);
        }

        [[nodiscard]] bool validateStackAnalyzerMode(const std::string& value,
                                                     const std::string& locationPath,
                                                     std::string& errorMessage)
        {
            const std::string lowered = toLower(trimCopy(value));
            if (value.empty() || lowered == "ir" || lowered == "abi")
            {
                return true;
            }
            return invalidValue(value, locationPath, "ir, abi", errorMessage);
        }

        [[nodiscard]] bool validateOutputFormat(const std::string& value,
                                                const std::string& locationPath,
                                                std::string& errorMessage)
        {
            const std::string lowered = toLower(trimCopy(value));
            if (value.empty() || lowered == "human" || lowered == "json" || lowered == "sarif")
            {
                return true;
            }
            return invalidValue(value, locationPath, "human, json, sarif", errorMessage);
        }

        [[nodiscard]] bool validateJobsValue(const std::string& value,
                                             const std::string& locationPath,
                                             std::string& errorMessage)
        {
            if (value.empty())
            {
                return true;
            }
            const std::string trimmed = trimCopy(value);
            if (toLower(trimmed) == "auto")
            {
                return true;
            }
            const bool allDigits =
                !trimmed.empty() &&
                std::all_of(trimmed.begin(), trimmed.end(),
                            [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)); });
            if (!allDigits)
            {
                return invalidValue(value, locationPath, "auto, positive integer", errorMessage);
            }
            uint64_t parsed = 0;
            try
            {
                parsed = std::stoull(trimmed);
            }
            catch (const std::exception&)
            {
                return invalidValue(value, locationPath, "auto, positive integer", errorMessage);
            }
            if (parsed == 0)
            {
                errorMessage = "Invalid value '" + value + "' for '" + locationPath +
                               "'. jobs must be >= 1 or 'auto'.";
                return false;
            }
            return true;
        }

        // ---------------------------------------------------------------- schema table

        /// How a JSON value is read before it is applied.
        enum class Kind
        {
            Bool,         ///< JSON boolean only.
            BoolLike,     ///< Boolean or "true/false/on/off/1/0/yes/no".
            String,       ///< JSON string only.
            ScalarString, ///< String or integer, kept as text.
            Uint64,       ///< Unsigned integer.
            StringList    ///< String or array of strings.
        };

        struct Value
        {
            bool boolean = false;
            std::string text;
            uint64_t number = 0;
            std::vector<std::string> list;
        };

        struct LoadContext
        {
            ProgramConfig& config;
            const std::filesystem::path& configDir;
        };

        using ApplyFn = std::function<bool(const Value&, LoadContext&,
                                           const std::string& locationPath, std::string& error)>;

        /// One configuration key: its canonical name (used in diagnostics), the accepted
        /// spellings, how the value is read and how it lands in ProgramConfig.
        struct FieldSpec
        {
            const char* name;
            std::vector<const char*> keys;
            Kind kind;
            ApplyFn apply;
        };

        struct SectionSpec
        {
            std::vector<FieldSpec> fields;

            [[nodiscard]] std::vector<const char*> allowedKeys() const
            {
                std::vector<const char*> keys;
                for (const FieldSpec& field : fields)
                {
                    keys.insert(keys.end(), field.keys.begin(), field.keys.end());
                }
                return keys;
            }
        };

        [[nodiscard]] bool readValue(const json& raw, Kind kind, const std::string& locationPath,
                                     Value& out, std::string& errorMessage)
        {
            switch (kind)
            {
            case Kind::Bool:
                if (!raw.is_boolean())
                {
                    errorMessage = "Expected boolean for '" + locationPath + "'.";
                    return false;
                }
                out.boolean = raw.get<bool>();
                return true;
            case Kind::BoolLike:
                return parseBoolLike(raw, out.boolean, errorMessage, locationPath);
            case Kind::String:
                if (!raw.is_string())
                {
                    errorMessage = "Expected string for '" + locationPath + "'.";
                    return false;
                }
                out.text = raw.get<std::string>();
                return true;
            case Kind::ScalarString:
                if (raw.is_string())
                {
                    out.text = raw.get<std::string>();
                    return true;
                }
                if (raw.is_number_unsigned())
                {
                    out.text = std::to_string(raw.get<uint64_t>());
                    return true;
                }
                if (raw.is_number_integer())
                {
                    out.text = std::to_string(raw.get<int64_t>());
                    return true;
                }
                errorMessage = "Expected string or integer for '" + locationPath + "'.";
                return false;
            case Kind::Uint64:
                if (!raw.is_number_unsigned())
                {
                    errorMessage = "Expected unsigned integer for '" + locationPath + "'.";
                    return false;
                }
                out.number = raw.get<uint64_t>();
                return true;
            case Kind::StringList:
                return parseStringList(raw, out.list, errorMessage, locationPath);
            }
            return false;
        }

        /// Applies a section object: unknown keys are rejected, then each field is read from
        /// the first of its spellings that is present (a null value means "not set").
        [[nodiscard]] bool applySection(const json& section, const SectionSpec& spec,
                                        std::string_view location, LoadContext& ctx,
                                        std::string& errorMessage)
        {
            if (!validateKnownKeys(section, spec.allowedKeys(), location, errorMessage))
            {
                return false;
            }
            for (const FieldSpec& field : spec.fields)
            {
                const json* raw = nullptr;
                for (const char* key : field.keys)
                {
                    if (const auto it = section.find(key); it != section.end())
                    {
                        raw = &(*it);
                        break;
                    }
                }
                if (raw == nullptr || raw->is_null())
                {
                    continue;
                }
                const std::string locationPath = std::string(location) + "." + field.name;
                Value value;
                if (!readValue(*raw, field.kind, locationPath, value, errorMessage) ||
                    !field.apply(value, ctx, locationPath, errorMessage))
                {
                    return false;
                }
            }
            return true;
        }

        // Field factories: plain member assignments, with the config-directory path resolution
        // and enum validation where the schema requires them.

        template <typename Section, typename Member>
        [[nodiscard]] Member& at(ProgramConfig& config, Section ProgramConfig::* section,
                                 Member Section::* member)
        {
            return (config.*section).*member;
        }

        template <typename Section>
        [[nodiscard]] ApplyFn setBool(Section ProgramConfig::* section, bool Section::* member)
        {
            return [=](const Value& v, LoadContext& ctx, const std::string&, std::string&)
            {
                at(ctx.config, section, member) = v.boolean;
                return true;
            };
        }

        template <typename Section>
        [[nodiscard]] ApplyFn setOptionalBool(Section ProgramConfig::* section,
                                              std::optional<bool> Section::* member, bool negate)
        {
            return [=](const Value& v, LoadContext& ctx, const std::string&, std::string&)
            {
                at(ctx.config, section, member) = negate ? !v.boolean : v.boolean;
                return true;
            };
        }

        template <typename Section>
        [[nodiscard]] ApplyFn setString(Section ProgramConfig::* section,
                                        std::string Section::* member,
                                        Validator validator = nullptr)
        {
            return [=](const Value& v, LoadContext& ctx, const std::string& location,
                       std::string& error)
            {
                if (validator != nullptr && !validator(v.text, location, error))
                {
                    return false;
                }
                at(ctx.config, section, member) = v.text;
                return true;
            };
        }

        /// Relative paths are resolved from the config file directory; empty stays empty.
        template <typename Section>
        [[nodiscard]] ApplyFn setPath(Section ProgramConfig::* section,
                                      std::string Section::* member)
        {
            return [=](const Value& v, LoadContext& ctx, const std::string&, std::string&)
            {
                at(ctx.config, section, member) =
                    v.text.empty() ? std::string()
                                   : resolvePathFromBase(ctx.configDir, v.text).string();
                return true;
            };
        }

        template <typename Section>
        [[nodiscard]] ApplyFn setList(Section ProgramConfig::* section,
                                      std::vector<std::string> Section::* member)
        {
            return [=](const Value& v, LoadContext& ctx, const std::string&, std::string&)
            {
                at(ctx.config, section, member) = v.list;
                return true;
            };
        }

        template <typename Section>
        [[nodiscard]] ApplyFn setUint64(Section ProgramConfig::* section,
                                        uint64_t Section::* member)
        {
            return [=](const Value& v, LoadContext& ctx, const std::string&, std::string&)
            {
                at(ctx.config, section, member) = v.number;
                return true;
            };
        }

        [[nodiscard]] bool applyInvoke(const Value& v, LoadContext& ctx,
                                       const std::string& location, std::string& error)
        {
            std::string normalizeError;
            const auto normalized = normalizeAndValidateToolList(v.list, normalizeError);
            if (!normalizeError.empty())
            {
                error = location + ": " + normalizeError;
                return false;
            }
            ctx.config.analysis.invoke = normalized;
            return true;
        }

        void assignInputFiles(const std::vector<std::string>& entries, LoadContext& ctx)
        {
            ctx.config.files.input.clear();
            for (const auto& entry : entries)
            {
                if (!entry.empty())
                {
                    ctx.config.files.input.emplace_back(
                        resolvePathFromBase(ctx.configDir, entry).string());
                }
            }
        }

        [[nodiscard]] ApplyFn setBoundedInt(int ServerConfig::* member, uint64_t max,
                                            const char* tooLargeMessage)
        {
            return [=](const Value& v, LoadContext& ctx, const std::string&, std::string& error)
            {
                if (v.number > static_cast<uint64_t>(std::numeric_limits<int>::max()) ||
                    v.number > max)
                {
                    error = tooLargeMessage;
                    return false;
                }
                ctx.config.server.*member = static_cast<int>(v.number);
                return true;
            };
        }

        // The tables below are the schema: docs/configuration.md describes the same keys.

        const SectionSpec& analysisSection()
        {
            static const SectionSpec spec{{
                {"static",
                 {"static", "static_analysis"},
                 Kind::Bool,
                 setBool(&ProgramConfig::analysis, &AnalysisConfig::static_enabled)},
                {"dynamic",
                 {"dynamic", "dynamic_analysis"},
                 Kind::Bool,
                 setBool(&ProgramConfig::analysis, &AnalysisConfig::dynamic_enabled)},
                {"invoke", {"invoke"}, Kind::StringList, applyInvoke},
            }};
            return spec;
        }

        const SectionSpec& filesSection()
        {
            static const SectionSpec spec{{
                {"input",
                 {"input"},
                 Kind::StringList,
                 [](const Value& v, LoadContext& ctx, const std::string&, std::string&)
                 {
                     assignInputFiles(v.list, ctx);
                     return true;
                 }},
                {"entry_points",
                 {"entry_points"},
                 Kind::StringList,
                 setList(&ProgramConfig::files, &FilesConfig::entry_points)},
                {"compile_commands",
                 {"compile_commands"},
                 Kind::String,
                 setPath(&ProgramConfig::files, &FilesConfig::compile_commands)},
                {"include_compdb_deps",
                 {"include_compdb_deps"},
                 Kind::Bool,
                 setBool(&ProgramConfig::files, &FilesConfig::include_compdb_deps)},
            }};
            return spec;
        }

        const SectionSpec& outputSection()
        {
            static const SectionSpec spec{{
                {"sarif_format",
                 {"sarif_format"},
                 Kind::Bool,
                 setBool(&ProgramConfig::output, &OutputConfig::sarif_format)},
                {"report_file",
                 {"report_file"},
                 Kind::String,
                 setString(&ProgramConfig::output, &OutputConfig::report_file)},
                {"output_file",
                 {"output_file"},
                 Kind::String,
                 setString(&ProgramConfig::output, &OutputConfig::output_file)},
                {"verbose",
                 {"verbose"},
                 Kind::Bool,
                 setBool(&ProgramConfig::output, &OutputConfig::verbose)},
                {"quiet",
                 {"quiet"},
                 Kind::Bool,
                 setBool(&ProgramConfig::output, &OutputConfig::quiet)},
                {"demangle",
                 {"demangle"},
                 Kind::Bool,
                 setBool(&ProgramConfig::output, &OutputConfig::demangle)},
            }};
            return spec;
        }

        const SectionSpec& runtimeSection()
        {
            static const SectionSpec spec{{
                {"async",
                 {"async"},
                 Kind::Bool,
                 setBool(&ProgramConfig::runtime, &RuntimeConfig::async)},
                {"ipc",
                 {"ipc"},
                 Kind::String,
                 setString(&ProgramConfig::runtime, &RuntimeConfig::ipc, validateIpcValue)},
                {"ipc_path",
                 {"ipc_path"},
                 Kind::String,
                 setString(&ProgramConfig::runtime, &RuntimeConfig::ipc_path)},
            }};
            return spec;
        }

        const SectionSpec& serverSection()
        {
            static const SectionSpec spec{{
                {"host",
                 {"host"},
                 Kind::String,
                 setString(&ProgramConfig::server, &ServerConfig::host)},
                {"port",
                 {"port"},
                 Kind::Uint64,
                 setBoundedInt(&ServerConfig::port, 65535U,
                               "server.port must be between 0 and 65535.")},
                {"shutdown_token",
                 {"shutdown_token"},
                 Kind::String,
                 setString(&ProgramConfig::server, &ServerConfig::shutdown_token)},
                {"shutdown_timeout_ms",
                 {"shutdown_timeout_ms"},
                 Kind::Uint64,
                 setBoundedInt(&ServerConfig::shutdown_timeout_ms,
                               std::numeric_limits<uint64_t>::max(),
                               "server.shutdown_timeout_ms is too large.")},
            }};
            return spec;
        }

        const SectionSpec& stackAnalyzerSection()
        {
            using SA = StackAnalyzerConfig;
            constexpr auto sa = &ProgramConfig::stack_analyzer;
            static const SectionSpec spec{{
                {"mode",
                 {"mode"},
                 Kind::String,
                 setString(sa, &SA::mode, validateStackAnalyzerMode)},
                {"output_format",
                 {"output_format", "output-format", "format"},
                 Kind::String,
                 setString(sa, &SA::output_format, validateOutputFormat)},
                {"config", {"config"}, Kind::String, setPath(sa, &SA::config)},
                {"print_effective_config",
                 {"print_effective_config", "print-effective-config"},
                 Kind::Bool,
                 setBool(sa, &SA::print_effective_config)},
                {"extra_args", {"extra_args"}, Kind::StringList, setList(sa, &SA::extra_args)},
                {"compile_commands",
                 {"compile_commands", "compile-commands", "compdb"},
                 Kind::String,
                 setPath(&ProgramConfig::files, &FilesConfig::compile_commands)},
                {"compile_args",
                 {"compile_args", "compile-args", "compile_arg", "compile-arg"},
                 Kind::StringList,
                 setList(sa, &SA::compile_args)},
                {"include_dirs",
                 {"include_dirs", "include-dirs", "include_dir", "include-dir"},
                 Kind::StringList,
                 setList(sa, &SA::include_dirs)},
                {"defines", {"defines", "define"}, Kind::StringList, setList(sa, &SA::defines)},
                {"resource_model",
                 {"resource_model", "resource-model"},
                 Kind::String,
                 setPath(sa, &SA::resource_model)},
                {"escape_model",
                 {"escape_model", "escape-model"},
                 Kind::String,
                 setPath(sa, &SA::escape_model)},
                {"buffer_model",
                 {"buffer_model", "buffer-model"},
                 Kind::String,
                 setPath(sa, &SA::buffer_model)},
                {"demangle",
                 {"demangle"},
                 Kind::Bool,
                 setBool(&ProgramConfig::output, &OutputConfig::demangle)},
                {"verbose",
                 {"verbose"},
                 Kind::Bool,
                 setBool(&ProgramConfig::output, &OutputConfig::verbose)},
                {"timing", {"timing"}, Kind::Bool, setBool(sa, &SA::timing)},
                {"include_compdb_deps",
                 {"include_compdb_deps", "include-compdb-deps"},
                 Kind::Bool,
                 setBool(&ProgramConfig::files, &FilesConfig::include_compdb_deps)},
                {"compdb_fast",
                 {"compdb_fast", "compdb-fast"},
                 Kind::Bool,
                 setBool(sa, &SA::compdb_fast)},
                {"quiet",
                 {"quiet"},
                 Kind::Bool,
                 setBool(&ProgramConfig::output, &OutputConfig::quiet)},
                {"stack_limit",
                 {"stack_limit", "stack-limit"},
                 Kind::Uint64,
                 setUint64(sa, &SA::stack_limit)},
                {"jobs",
                 {"jobs"},
                 Kind::ScalarString,
                 [](const Value& v, LoadContext& ctx, const std::string& location,
                    std::string& error)
                 {
                     if (!validateJobsValue(v.text, location, error))
                     {
                         return false;
                     }
                     ctx.config.stack_analyzer.jobs = trimCopy(v.text);
                     return true;
                 }},
                {"analysis_profile",
                 {"analysis-profile", "analysis_profile"},
                 Kind::String,
                 setString(sa, &SA::analysis_profile, validateAnalysisProfile)},
                {"smt",
                 {"smt"},
                 Kind::BoolLike,
                 [](const Value& v, LoadContext& ctx, const std::string&, std::string&)
                 {
                     ctx.config.stack_analyzer.smt = v.boolean ? "on" : "off";
                     return true;
                 }},
                {"smt_backend",
                 {"smt-backend", "smt_backend"},
                 Kind::String,
                 setString(sa, &SA::smt_backend)},
                {"smt_secondary_backend",
                 {"smt-secondary-backend", "smt_secondary_backend"},
                 Kind::String,
                 setString(sa, &SA::smt_secondary_backend)},
                {"smt_mode",
                 {"smt-mode", "smt_mode"},
                 Kind::String,
                 setString(sa, &SA::smt_mode, validateSmtMode)},
                {"smt_timeout_ms",
                 {"smt-timeout-ms", "smt_timeout_ms"},
                 Kind::Uint64,
                 [](const Value& v, LoadContext& ctx, const std::string& location,
                    std::string& error)
                 {
                     if (v.number > std::numeric_limits<uint32_t>::max())
                     {
                         error = location + " is too large.";
                         return false;
                     }
                     ctx.config.stack_analyzer.smt_timeout_ms = static_cast<uint32_t>(v.number);
                     return true;
                 }},
                {"smt_budget_nodes",
                 {"smt-budget-nodes", "smt_budget_nodes"},
                 Kind::Uint64,
                 setUint64(sa, &SA::smt_budget_nodes)},
                {"smt_rules",
                 {"smt-rules", "smt_rules"},
                 Kind::StringList,
                 setList(sa, &SA::smt_rules)},
                {"only_functions",
                 {"only_functions", "only-functions", "only_function", "only-function", "only_func",
                  "only-func"},
                 Kind::StringList,
                 setList(sa, &SA::only_functions)},
                {"only_files",
                 {"only_files", "only-files", "only_file", "only-file"},
                 Kind::StringList,
                 setList(sa, &SA::only_files)},
                {"only_dirs",
                 {"only_dirs", "only-dirs", "only_dir", "only-dir"},
                 Kind::StringList,
                 setList(sa, &SA::only_dirs)},
                {"exclude_dirs",
                 {"exclude_dirs", "exclude-dirs", "exclude_dir", "exclude-dir"},
                 Kind::StringList,
                 setList(sa, &SA::exclude_dirs)},
                // Legacy spelling: analyzer entry points also drive the function filter.
                {"entry_points",
                 {"entry_points"},
                 Kind::StringList,
                 [](const Value& v, LoadContext& ctx, const std::string&, std::string&)
                 {
                     ctx.config.files.entry_points = v.list;
                     ctx.config.stack_analyzer.only_functions = v.list;
                     return true;
                 }},
                {"resource_cross_tu",
                 {"resource_cross_tu", "resource-cross-tu"},
                 Kind::Bool,
                 setOptionalBool(sa, &SA::resource_cross_tu, false)},
                {"no_resource_cross_tu",
                 {"no_resource_cross_tu", "no-resource-cross-tu"},
                 Kind::Bool,
                 setOptionalBool(sa, &SA::resource_cross_tu, true)},
                {"uninitialized_cross_tu",
                 {"uninitialized_cross_tu", "uninitialized-cross-tu"},
                 Kind::Bool,
                 setOptionalBool(sa, &SA::uninitialized_cross_tu, false)},
                {"no_uninitialized_cross_tu",
                 {"no_uninitialized_cross_tu", "no-uninitialized-cross-tu"},
                 Kind::Bool,
                 setOptionalBool(sa, &SA::uninitialized_cross_tu, true)},
                {"resource_summary_cache_dir",
                 {"resource_summary_cache_dir", "resource-summary-cache-dir"},
                 Kind::String,
                 setPath(sa, &SA::resource_summary_cache_dir)},
                {"resource_summary_cache_memory_only",
                 {"resource_summary_cache_memory_only", "resource-summary-cache-memory-only"},
                 Kind::Bool,
                 setBool(sa, &SA::resource_summary_cache_memory_only)},
                {"compile_ir_cache_dir",
                 {"compile_ir_cache_dir", "compile-ir-cache-dir"},
                 Kind::String,
                 setPath(sa, &SA::compile_ir_cache_dir)},
                {"compile_ir_format",
                 {"compile_ir_format", "compile-ir-format"},
                 Kind::String,
                 setString(sa, &SA::compile_ir_format, validateCompileIRFormat)},
                {"include_stl",
                 {"include_stl", "include-stl", "stl"},
                 Kind::Bool,
                 setBool(sa, &SA::include_stl)},
                {"base_dir", {"base_dir", "base-dir"}, Kind::String, setPath(sa, &SA::base_dir)},
                {"dump_filter",
                 {"dump_filter", "dump-filter"},
                 Kind::Bool,
                 setBool(sa, &SA::dump_filter)},
                {"dump_ir", {"dump_ir", "dump-ir"}, Kind::String, setPath(sa, &SA::dump_ir)},
                {"warnings_only",
                 {"warnings_only", "warnings-only"},
                 Kind::Bool,
                 setBool(sa, &SA::warnings_only)},
            }};
            return spec;
        }

        // ------------------------------------------------------------- root layout

        [[nodiscard]] bool applyLegacyRootInvokeAndInput(const json& root, LoadContext& ctx,
                                                         std::string& errorMessage)
        {
            if (const auto it = root.find("invoke"); it != root.end() && !it->is_null())
            {
                Value value;
                if (!readValue(*it, Kind::StringList, "invoke", value, errorMessage) ||
                    !applyInvoke(value, ctx, "invoke", errorMessage))
                {
                    return false;
                }
            }
            if (const auto it = root.find("input"); it != root.end() && !it->is_null())
            {
                if (it->is_object())
                {
                    errorMessage = "Expected string or array of strings for 'input'.";
                    return false;
                }
                Value value;
                if (!readValue(*it, Kind::StringList, "input", value, errorMessage))
                {
                    return false;
                }
                assignInputFiles(value.list, ctx);
            }
            return true;
        }

        /// `stack_analyzer`, `stack-analyzer`, `tools.ctrace_stack_analyzer` or
        /// `tools.stack_analyzer`, in that order of preference.
        [[nodiscard]] const json* findStackAnalyzerSection(const json& root,
                                                           std::string& errorMessage)
        {
            const auto asObject = [&](const json& value, const char* name) -> const json*
            {
                if (!value.is_object())
                {
                    errorMessage = "Expected object for '" + std::string(name) + "'.";
                    return nullptr;
                }
                return &value;
            };
            for (const char* key : {"stack_analyzer", "stack-analyzer"})
            {
                if (const auto it = root.find(key); it != root.end() && !it->is_null())
                {
                    return asObject(*it, key);
                }
            }
            const auto itTools = root.find("tools");
            if (itTools == root.end() || itTools->is_null())
            {
                return nullptr;
            }
            if (!itTools->is_object())
            {
                errorMessage = "Expected object for 'tools'.";
                return nullptr;
            }
            if (!validateKnownKeys(*itTools, {"ctrace_stack_analyzer", "stack_analyzer"}, "tools",
                                   errorMessage))
            {
                return nullptr;
            }
            for (const char* key : {"ctrace_stack_analyzer", "stack_analyzer"})
            {
                if (const auto it = itTools->find(key); it != itTools->end() && !it->is_null())
                {
                    return asObject(*it, (std::string("tools.") + key).c_str());
                }
            }
            return nullptr;
        }

        [[nodiscard]] bool applySchemaVersion(const json& root, std::string& errorMessage)
        {
            const auto it = root.find("schema_version");
            if (it == root.end() || it->is_null())
            {
                return true;
            }
            if (!it->is_number_unsigned())
            {
                errorMessage = "Expected unsigned integer for 'schema_version'.";
                return false;
            }
            const auto version = it->get<uint64_t>();
            if (version != kToolConfigSchemaVersion)
            {
                errorMessage = "Unsupported schema_version '" + std::to_string(version) +
                               "'. Supported version: " + std::to_string(kToolConfigSchemaVersion) +
                               ".";
                return false;
            }
            return true;
        }

        [[nodiscard]] bool applyNamedSection(const json& root, const char* name,
                                             const SectionSpec& spec, LoadContext& ctx,
                                             std::string& errorMessage)
        {
            const auto it = root.find(name);
            if (it == root.end() || it->is_null())
            {
                return true;
            }
            if (!it->is_object())
            {
                errorMessage = "Expected object for '" + std::string(name) + "'.";
                return false;
            }
            return applySection(*it, spec, name, ctx, errorMessage);
        }
    } // namespace

    bool applyToolConfigFile(ProgramConfig& config, std::string_view configPath,
                             std::string& errorMessage)
    {
        errorMessage.clear();
        if (configPath.empty())
        {
            errorMessage = "Config path is empty.";
            return false;
        }

        const std::filesystem::path path(configPath);
        json root;
        if (!loadJsonFile(path, root, errorMessage))
        {
            return false;
        }
        if (!validateKnownKeys(root,
                               {"schema_version", "analysis", "files", "output", "runtime",
                                "server", "stack_analyzer", "stack-analyzer", "invoke", "input",
                                "tools"},
                               "root", errorMessage))
        {
            return false;
        }
        if (!applySchemaVersion(root, errorMessage))
        {
            return false;
        }

        const std::filesystem::path configDir = std::filesystem::absolute(path).parent_path();
        config.config_file = path.lexically_normal().string();
        LoadContext ctx{config, configDir};

        // Precedence inside the file: analyzer section, then legacy root keys, then the
        // canonical sections, which therefore win (docs/configuration.md, Legacy Compatibility).
        if (const json* analyzerSection = findStackAnalyzerSection(root, errorMessage);
            analyzerSection != nullptr)
        {
            if (!applySection(*analyzerSection, stackAnalyzerSection(), "stack_analyzer", ctx,
                              errorMessage))
            {
                return false;
            }
        }
        else if (!errorMessage.empty())
        {
            return false;
        }

        if (!applyLegacyRootInvokeAndInput(root, ctx, errorMessage))
        {
            return false;
        }

        return applyNamedSection(root, "analysis", analysisSection(), ctx, errorMessage) &&
               applyNamedSection(root, "files", filesSection(), ctx, errorMessage) &&
               applyNamedSection(root, "output", outputSection(), ctx, errorMessage) &&
               applyNamedSection(root, "runtime", runtimeSection(), ctx, errorMessage) &&
               applyNamedSection(root, "server", serverSection(), ctx, errorMessage);
    }
} // namespace ctrace
