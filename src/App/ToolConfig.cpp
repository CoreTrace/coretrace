// SPDX-License-Identifier: Apache-2.0
#include "App/ToolConfig.hpp"

#include "App/SupportedTools.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
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

        [[nodiscard]] std::string joinAllowed(std::initializer_list<const char*> keys)
        {
            std::string joined;
            bool first = true;
            for (const auto* key : keys)
            {
                if (key == nullptr)
                {
                    continue;
                }
                if (!first)
                {
                    joined += ", ";
                }
                first = false;
                joined += key;
            }
            return joined;
        }

        [[nodiscard]] bool validateKnownKeys(const json& object,
                                             std::initializer_list<const char*> allowedKeys,
                                             std::string_view context, std::string& errorMessage)
        {
            if (!object.is_object())
            {
                errorMessage = "Expected JSON object for '" + std::string(context) + "'.";
                return false;
            }

            std::unordered_set<std::string> allowed;
            allowed.reserve(allowedKeys.size());
            for (const auto* key : allowedKeys)
            {
                if (key != nullptr)
                {
                    allowed.emplace(key);
                }
            }

            for (auto it = object.begin(); it != object.end(); ++it)
            {
                if (allowed.find(it.key()) == allowed.end())
                {
                    errorMessage = "Unknown key '" + it.key() + "' in '" + std::string(context) +
                                   "'. Allowed keys: [" + joinAllowed(allowedKeys) + "]";
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

        [[nodiscard]] const json* findFirstValueForKeys(const json& object,
                                                        std::initializer_list<const char*> keys)
        {
            for (const auto* key : keys)
            {
                if (key == nullptr)
                {
                    continue;
                }
                const auto it = object.find(key);
                if (it != object.end())
                {
                    return &(*it);
                }
            }
            return nullptr;
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

        [[nodiscard]] bool parseUint64(const json& value, uint64_t& out, std::string& errorMessage,
                                       const std::string& locationPath)
        {
            if (!value.is_number_unsigned())
            {
                errorMessage = "Expected unsigned integer for '" + locationPath + "'.";
                return false;
            }
            out = value.get<uint64_t>();
            return true;
        }

        [[nodiscard]] bool readOptionalStringAny(const json& object,
                                                 std::initializer_list<const char*> keys,
                                                 std::string& out, std::string& errorMessage,
                                                 const std::string& locationPath, bool& hasValue)
        {
            const auto* value = findFirstValueForKeys(object, keys);
            if (value == nullptr || value->is_null())
            {
                hasValue = false;
                return true;
            }
            if (!value->is_string())
            {
                errorMessage = "Expected string for '" + locationPath + "'.";
                return false;
            }
            out = value->get<std::string>();
            hasValue = true;
            return true;
        }

        [[nodiscard]] bool
        readOptionalScalarAsStringAny(const json& object, std::initializer_list<const char*> keys,
                                      std::string& out, std::string& errorMessage,
                                      const std::string& locationPath, bool& hasValue)
        {
            const auto* value = findFirstValueForKeys(object, keys);
            if (value == nullptr || value->is_null())
            {
                hasValue = false;
                return true;
            }

            if (value->is_string())
            {
                out = value->get<std::string>();
                hasValue = true;
                return true;
            }
            if (value->is_number_unsigned())
            {
                out = std::to_string(value->get<uint64_t>());
                hasValue = true;
                return true;
            }
            if (value->is_number_integer())
            {
                out = std::to_string(value->get<int64_t>());
                hasValue = true;
                return true;
            }

            errorMessage = "Expected string or integer for '" + locationPath + "'.";
            return false;
        }

        [[nodiscard]] bool readOptionalBoolAny(const json& object,
                                               std::initializer_list<const char*> keys, bool& out,
                                               std::string& errorMessage,
                                               const std::string& locationPath, bool& hasValue)
        {
            const auto* value = findFirstValueForKeys(object, keys);
            if (value == nullptr || value->is_null())
            {
                hasValue = false;
                return true;
            }
            if (!value->is_boolean())
            {
                errorMessage = "Expected boolean for '" + locationPath + "'.";
                return false;
            }
            out = value->get<bool>();
            hasValue = true;
            return true;
        }

        [[nodiscard]] bool readOptionalBoolLikeAny(const json& object,
                                                   std::initializer_list<const char*> keys,
                                                   bool& out, std::string& errorMessage,
                                                   const std::string& locationPath, bool& hasValue)
        {
            const auto* value = findFirstValueForKeys(object, keys);
            if (value == nullptr || value->is_null())
            {
                hasValue = false;
                return true;
            }
            hasValue = true;
            return parseBoolLike(*value, out, errorMessage, locationPath);
        }

        [[nodiscard]] bool readOptionalUint64Any(const json& object,
                                                 std::initializer_list<const char*> keys,
                                                 uint64_t& out, std::string& errorMessage,
                                                 const std::string& locationPath, bool& hasValue)
        {
            const auto* value = findFirstValueForKeys(object, keys);
            if (value == nullptr || value->is_null())
            {
                hasValue = false;
                return true;
            }
            hasValue = true;
            return parseUint64(*value, out, errorMessage, locationPath);
        }

        [[nodiscard]] bool
        readOptionalStringListAny(const json& object, std::initializer_list<const char*> keys,
                                  std::vector<std::string>& out, std::string& errorMessage,
                                  const std::string& locationPath, bool& hasValue)
        {
            const auto* value = findFirstValueForKeys(object, keys);
            if (value == nullptr || value->is_null())
            {
                hasValue = false;
                return true;
            }
            hasValue = true;
            return parseStringList(*value, out, errorMessage, locationPath);
        }

        [[nodiscard]] bool parseEntryPoints(const std::vector<std::string>& points,
                                            ProgramConfig& config)
        {
            std::string joined;
            for (std::size_t i = 0; i < points.size(); ++i)
            {
                if (i > 0)
                {
                    joined.push_back(',');
                }
                joined += points[i];
            }
            config.global.entry_points = joined;
            return true;
        }

        void assignInputFiles(const std::vector<std::string>& entries,
                              const std::filesystem::path& configDir, ProgramConfig& config,
                              bool replace)
        {
            if (replace)
            {
                config.files.clear();
            }

            for (const auto& entry : entries)
            {
                if (entry.empty())
                {
                    continue;
                }
                const auto resolved = resolvePathFromBase(configDir, entry);
                config.files.emplace_back(resolved.string());
            }
        }

        [[nodiscard]] bool validateIpcValue(const std::string& ipcValue, std::string& errorMessage,
                                            const std::string& locationPath)
        {
            if (ipcValue.empty())
            {
                return true;
            }
            const auto& ipcList = ctrace_defs::IPC_TYPES;
            if (std::find(ipcList.begin(), ipcList.end(), ipcValue) != ipcList.end())
            {
                return true;
            }

            std::string allowed;
            for (std::size_t i = 0; i < ipcList.size(); ++i)
            {
                if (i > 0)
                {
                    allowed += ", ";
                }
                allowed += ipcList[i];
            }
            errorMessage = "Invalid value '" + ipcValue + "' for '" + locationPath +
                           "'. Allowed values: [" + allowed + "]";
            return false;
        }

        [[nodiscard]] bool validateAnalysisProfile(const std::string& value,
                                                   const std::string& locationPath,
                                                   std::string& errorMessage)
        {
            if (value.empty())
            {
                return true;
            }
            if (value == "fast" || value == "full")
            {
                return true;
            }
            errorMessage = "Invalid value '" + value + "' for '" + locationPath +
                           "'. Allowed values: [fast, full]";
            return false;
        }

        [[nodiscard]] bool validateSmtMode(const std::string& value,
                                           const std::string& locationPath,
                                           std::string& errorMessage)
        {
            if (value.empty())
            {
                return true;
            }
            static const std::unordered_set<std::string> allowed = {
                "single",
                "portfolio",
                "cross-check",
                "dual-consensus",
            };
            if (allowed.find(value) != allowed.end())
            {
                return true;
            }
            errorMessage = "Invalid value '" + value + "' for '" + locationPath +
                           "'. Allowed values: [single, portfolio, cross-check, dual-consensus]";
            return false;
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

            errorMessage = "Invalid value '" + value + "' for '" + locationPath +
                           "'. Allowed values: [bc, ll]";
            return false;
        }

        [[nodiscard]] bool validateStackAnalyzerMode(const std::string& value,
                                                     const std::string& locationPath,
                                                     std::string& errorMessage)
        {
            if (value.empty())
            {
                return true;
            }

            const std::string lowered = toLower(trimCopy(value));
            if (lowered == "ir" || lowered == "abi")
            {
                return true;
            }

            errorMessage = "Invalid value '" + value + "' for '" + locationPath +
                           "'. Allowed values: [ir, abi]";
            return false;
        }

        [[nodiscard]] bool validateOutputFormat(const std::string& value,
                                                const std::string& locationPath,
                                                std::string& errorMessage)
        {
            if (value.empty())
            {
                return true;
            }

            const std::string lowered = toLower(trimCopy(value));
            if (lowered == "human" || lowered == "json" || lowered == "sarif")
            {
                return true;
            }

            errorMessage = "Invalid value '" + value + "' for '" + locationPath +
                           "'. Allowed values: [human, json, sarif]";
            return false;
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
            const std::string lowered = toLower(trimmed);
            if (lowered == "auto")
            {
                return true;
            }

            if (trimmed.empty())
            {
                errorMessage = "Invalid value '" + value + "' for '" + locationPath +
                               "'. Allowed values: [auto, positive integer]";
                return false;
            }

            for (char ch : trimmed)
            {
                if (!std::isdigit(static_cast<unsigned char>(ch)))
                {
                    errorMessage = "Invalid value '" + value + "' for '" + locationPath +
                                   "'. Allowed values: [auto, positive integer]";
                    return false;
                }
            }

            uint64_t parsed = 0;
            try
            {
                parsed = std::stoull(trimmed);
            }
            catch (const std::exception&)
            {
                errorMessage = "Invalid value '" + value + "' for '" + locationPath +
                               "'. Allowed values: [auto, positive integer]";
                return false;
            }

            if (parsed == 0)
            {
                errorMessage = "Invalid value '" + value + "' for '" + locationPath +
                               "'. jobs must be >= 1 or 'auto'.";
                return false;
            }

            return true;
        }

        [[nodiscard]] bool applyInvokeValue(const json& value, ProgramConfig& config,
                                            std::string& errorMessage,
                                            const std::string& locationPath)
        {
            std::vector<std::string> tools;
            if (!parseStringList(value, tools, errorMessage, locationPath))
            {
                return false;
            }

            std::string normalizeError;
            const auto normalized = normalizeAndValidateToolList(tools, normalizeError);
            if (!normalizeError.empty())
            {
                errorMessage = locationPath + ": " + normalizeError;
                return false;
            }

            config.global.specificTools = normalized;
            config.global.hasInvokedSpecificTools = !normalized.empty();
            return true;
        }

        [[nodiscard]] bool applyLegacyRootInvokeAndInput(const json& root,
                                                         const std::filesystem::path& configDir,
                                                         ProgramConfig& config,
                                                         std::string& errorMessage)
        {
            if (const auto it = root.find("invoke"); it != root.end() && !it->is_null())
            {
                if (!applyInvokeValue(*it, config, errorMessage, "invoke"))
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
                std::vector<std::string> entries;
                if (!parseStringList(*it, entries, errorMessage, "input"))
                {
                    return false;
                }
                assignInputFiles(entries, configDir, config, true);
            }

            return true;
        }

        [[nodiscard]] bool applyAnalysisSection(const json& section, ProgramConfig& config,
                                                std::string& errorMessage)
        {
            if (!validateKnownKeys(section,
                                   {
                                       "static",
                                       "dynamic",
                                       "static_analysis",
                                       "dynamic_analysis",
                                       "invoke",
                                   },
                                   "analysis", errorMessage))
            {
                return false;
            }

            bool boolValue = false;
            bool hasValue = false;
            if (!readOptionalBoolAny(section, {"static", "static_analysis"}, boolValue,
                                     errorMessage, "analysis.static", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.hasStaticAnalysis = boolValue;
            }

            if (!readOptionalBoolAny(section, {"dynamic", "dynamic_analysis"}, boolValue,
                                     errorMessage, "analysis.dynamic", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.hasDynamicAnalysis = boolValue;
            }

            if (const auto it = section.find("invoke"); it != section.end() && !it->is_null())
            {
                if (!applyInvokeValue(*it, config, errorMessage, "analysis.invoke"))
                {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] bool applyFilesSection(const json& section,
                                             const std::filesystem::path& configDir,
                                             ProgramConfig& config, std::string& errorMessage)
        {
            if (!validateKnownKeys(section,
                                   {
                                       "input",
                                       "entry_points",
                                       "compile_commands",
                                       "include_compdb_deps",
                                   },
                                   "files", errorMessage))
            {
                return false;
            }

            bool hasValue = false;
            std::vector<std::string> listValue;
            if (!readOptionalStringListAny(section, {"input"}, listValue, errorMessage,
                                           "files.input", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                assignInputFiles(listValue, configDir, config, true);
            }

            if (!readOptionalStringListAny(section, {"entry_points"}, listValue, errorMessage,
                                           "files.entry_points", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                (void)parseEntryPoints(listValue, config);
            }

            std::string stringValue;
            if (!readOptionalStringAny(section, {"compile_commands"}, stringValue, errorMessage,
                                       "files.compile_commands", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.compile_commands =
                    stringValue.empty() ? std::string()
                                        : resolvePathFromBase(configDir, stringValue).string();
            }

            bool boolValue = false;
            if (!readOptionalBoolAny(section, {"include_compdb_deps"}, boolValue, errorMessage,
                                     "files.include_compdb_deps", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.include_compdb_deps = boolValue;
            }

            return true;
        }

        [[nodiscard]] bool applyOutputSection(const json& section, ProgramConfig& config,
                                              std::string& errorMessage)
        {
            if (!validateKnownKeys(section,
                                   {
                                       "sarif_format",
                                       "report_file",
                                       "output_file",
                                       "verbose",
                                       "quiet",
                                       "demangle",
                                   },
                                   "output", errorMessage))
            {
                return false;
            }

            bool hasValue = false;
            bool boolValue = false;
            if (!readOptionalBoolAny(section, {"sarif_format"}, boolValue, errorMessage,
                                     "output.sarif_format", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.hasSarifFormat = boolValue;
            }

            if (!readOptionalBoolAny(section, {"verbose"}, boolValue, errorMessage,
                                     "output.verbose", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.verbose = boolValue;
            }

            if (!readOptionalBoolAny(section, {"quiet"}, boolValue, errorMessage, "output.quiet",
                                     hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.quiet = boolValue;
            }

            if (!readOptionalBoolAny(section, {"demangle"}, boolValue, errorMessage,
                                     "output.demangle", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.demangle = boolValue;
            }

            std::string stringValue;
            if (!readOptionalStringAny(section, {"report_file"}, stringValue, errorMessage,
                                       "output.report_file", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.report_file = stringValue;
            }

            if (!readOptionalStringAny(section, {"output_file"}, stringValue, errorMessage,
                                       "output.output_file", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.output_file = stringValue;
            }

            return true;
        }

        [[nodiscard]] bool applyRuntimeSection(const json& section, ProgramConfig& config,
                                               std::string& errorMessage)
        {
            if (!validateKnownKeys(section,
                                   {
                                       "async",
                                       "ipc",
                                       "ipc_path",
                                   },
                                   "runtime", errorMessage))
            {
                return false;
            }

            bool hasValue = false;
            bool boolValue = false;
            if (!readOptionalBoolAny(section, {"async"}, boolValue, errorMessage, "runtime.async",
                                     hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.hasAsync = boolValue ? std::launch::async : std::launch::deferred;
            }

            std::string stringValue;
            if (!readOptionalStringAny(section, {"ipc"}, stringValue, errorMessage, "runtime.ipc",
                                       hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                if (!validateIpcValue(stringValue, errorMessage, "runtime.ipc"))
                {
                    return false;
                }
                config.global.ipc = stringValue;
            }

            if (!readOptionalStringAny(section, {"ipc_path"}, stringValue, errorMessage,
                                       "runtime.ipc_path", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                if (!stringValue.empty())
                {
                    config.global.ipcPath = stringValue;
                }
            }

            return true;
        }

        [[nodiscard]] bool applyServerSection(const json& section, ProgramConfig& config,
                                              std::string& errorMessage)
        {
            if (!validateKnownKeys(section,
                                   {
                                       "host",
                                       "port",
                                       "shutdown_token",
                                       "shutdown_timeout_ms",
                                   },
                                   "server", errorMessage))
            {
                return false;
            }

            bool hasValue = false;
            std::string stringValue;
            if (!readOptionalStringAny(section, {"host"}, stringValue, errorMessage, "server.host",
                                       hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.serverHost = stringValue;
            }

            if (!readOptionalStringAny(section, {"shutdown_token"}, stringValue, errorMessage,
                                       "server.shutdown_token", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.shutdownToken = stringValue;
            }

            uint64_t uintValue = 0;
            if (!readOptionalUint64Any(section, {"port"}, uintValue, errorMessage, "server.port",
                                       hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                if (uintValue > static_cast<uint64_t>(std::numeric_limits<int>::max()) ||
                    uintValue > 65535U)
                {
                    errorMessage = "server.port must be between 0 and 65535.";
                    return false;
                }
                config.global.serverPort = static_cast<int>(uintValue);
            }

            if (!readOptionalUint64Any(section, {"shutdown_timeout_ms"}, uintValue, errorMessage,
                                       "server.shutdown_timeout_ms", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                if (uintValue > static_cast<uint64_t>(std::numeric_limits<int>::max()))
                {
                    errorMessage = "server.shutdown_timeout_ms is too large.";
                    return false;
                }
                config.global.shutdownTimeoutMs = static_cast<int>(uintValue);
            }

            return true;
        }

        [[nodiscard]] bool applyStackAnalyzerSection(const json& section,
                                                     const std::filesystem::path& configDir,
                                                     ProgramConfig& config,
                                                     std::string& errorMessage,
                                                     std::string_view location)
        {
            if (!validateKnownKeys(section,
                                   {
                                       "mode",
                                       "output_format",
                                       "output-format",
                                       "format",
                                       "config",
                                       "print_effective_config",
                                       "print-effective-config",
                                       "extra_args",
                                       "compile_commands",
                                       "compile-commands",
                                       "compdb",
                                       "compile_args",
                                       "compile-args",
                                       "compile_arg",
                                       "compile-arg",
                                       "include_dirs",
                                       "include-dirs",
                                       "include_dir",
                                       "include-dir",
                                       "defines",
                                       "define",
                                       "resource_model",
                                       "resource-model",
                                       "escape_model",
                                       "escape-model",
                                       "buffer_model",
                                       "buffer-model",
                                       "demangle",
                                       "verbose",
                                       "timing",
                                       "include_compdb_deps",
                                       "include-compdb-deps",
                                       "quiet",
                                       "compdb_fast",
                                       "compdb-fast",
                                       "stack_limit",
                                       "stack-limit",
                                       "analysis-profile",
                                       "analysis_profile",
                                       "smt",
                                       "smt-backend",
                                       "smt_backend",
                                       "smt-secondary-backend",
                                       "smt_secondary_backend",
                                       "smt-mode",
                                       "smt_mode",
                                       "smt-timeout-ms",
                                       "smt_timeout_ms",
                                       "smt-budget-nodes",
                                       "smt_budget_nodes",
                                       "smt-rules",
                                       "smt_rules",
                                       "entry_points",
                                       "only_functions",
                                       "only-functions",
                                       "only_function",
                                       "only-function",
                                       "only_func",
                                       "only-func",
                                       "only_files",
                                       "only-files",
                                       "only_file",
                                       "only-file",
                                       "only_dirs",
                                       "only-dirs",
                                       "only_dir",
                                       "only-dir",
                                       "exclude_dirs",
                                       "exclude-dirs",
                                       "exclude_dir",
                                       "exclude-dir",
                                       "jobs",
                                       "resource_cross_tu",
                                       "resource-cross-tu",
                                       "no_resource_cross_tu",
                                       "no-resource-cross-tu",
                                       "uninitialized_cross_tu",
                                       "uninitialized-cross-tu",
                                       "no_uninitialized_cross_tu",
                                       "no-uninitialized-cross-tu",
                                       "resource_summary_cache_dir",
                                       "resource-summary-cache-dir",
                                       "compile_ir_cache_dir",
                                       "compile-ir-cache-dir",
                                       "compile_ir_format",
                                       "compile-ir-format",
                                       "resource_summary_cache_memory_only",
                                       "resource-summary-cache-memory-only",
                                       "include_stl",
                                       "include-stl",
                                       "stl",
                                       "base_dir",
                                       "base-dir",
                                       "dump_filter",
                                       "dump-filter",
                                       "dump_ir",
                                       "dump-ir",
                                       "warnings_only",
                                       "warnings-only",
                                   },
                                   location, errorMessage))
            {
                return false;
            }

            bool hasValue = false;
            std::string stringValue;
            if (!readOptionalStringAny(section, {"mode"}, stringValue, errorMessage,
                                       std::string(location) + ".mode", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                if (!validateStackAnalyzerMode(stringValue, std::string(location) + ".mode",
                                               errorMessage))
                {
                    return false;
                }
                config.global.stack_analyzer_mode = stringValue;
            }

            if (!readOptionalStringAny(section, {"output_format", "output-format", "format"},
                                       stringValue, errorMessage,
                                       std::string(location) + ".output_format", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                if (!validateOutputFormat(stringValue, std::string(location) + ".output_format",
                                          errorMessage))
                {
                    return false;
                }
                config.global.stack_analyzer_output_format = stringValue;
            }

            if (!readOptionalStringAny(section, {"config"}, stringValue, errorMessage,
                                       std::string(location) + ".config", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_config =
                    stringValue.empty() ? std::string()
                                        : resolvePathFromBase(configDir, stringValue).string();
            }

            bool boolValue = false;
            if (!readOptionalBoolAny(section, {"print_effective_config", "print-effective-config"},
                                     boolValue, errorMessage,
                                     std::string(location) + ".print_effective_config", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_print_effective_config = boolValue;
            }

            std::vector<std::string> listValue;
            if (!readOptionalStringListAny(section, {"extra_args"}, listValue, errorMessage,
                                           std::string(location) + ".extra_args", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_extra_args = listValue;
            }

            if (!readOptionalStringAny(section, {"compile_commands", "compile-commands", "compdb"},
                                       stringValue, errorMessage,
                                       std::string(location) + ".compile_commands", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.compile_commands =
                    stringValue.empty() ? std::string()
                                        : resolvePathFromBase(configDir, stringValue).string();
            }

            if (!readOptionalStringListAny(
                    section, {"compile_args", "compile-args", "compile_arg", "compile-arg"},
                    listValue, errorMessage, std::string(location) + ".compile_args", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_compile_args = listValue;
            }

            if (!readOptionalStringListAny(
                    section, {"include_dirs", "include-dirs", "include_dir", "include-dir"},
                    listValue, errorMessage, std::string(location) + ".include_dirs", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_include_dirs = listValue;
            }

            if (!readOptionalStringListAny(section, {"defines", "define"}, listValue, errorMessage,
                                           std::string(location) + ".defines", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_defines = listValue;
            }

            if (!readOptionalStringAny(section, {"resource_model", "resource-model"}, stringValue,
                                       errorMessage, std::string(location) + ".resource_model",
                                       hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.resource_model =
                    stringValue.empty() ? std::string()
                                        : resolvePathFromBase(configDir, stringValue).string();
            }

            if (!readOptionalStringAny(section, {"escape_model", "escape-model"}, stringValue,
                                       errorMessage, std::string(location) + ".escape_model",
                                       hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.escape_model =
                    stringValue.empty() ? std::string()
                                        : resolvePathFromBase(configDir, stringValue).string();
            }

            if (!readOptionalStringAny(section, {"buffer_model", "buffer-model"}, stringValue,
                                       errorMessage, std::string(location) + ".buffer_model",
                                       hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.buffer_model =
                    stringValue.empty() ? std::string()
                                        : resolvePathFromBase(configDir, stringValue).string();
            }

            if (!readOptionalBoolAny(section, {"demangle"}, boolValue, errorMessage,
                                     std::string(location) + ".demangle", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.demangle = boolValue;
            }

            if (!readOptionalBoolAny(section, {"verbose"}, boolValue, errorMessage,
                                     std::string(location) + ".verbose", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.verbose = boolValue;
            }

            if (!readOptionalBoolAny(section, {"timing"}, boolValue, errorMessage,
                                     std::string(location) + ".timing", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.timing = boolValue;
            }

            if (!readOptionalBoolAny(section, {"include_compdb_deps", "include-compdb-deps"},
                                     boolValue, errorMessage,
                                     std::string(location) + ".include_compdb_deps", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.include_compdb_deps = boolValue;
            }

            if (!readOptionalBoolAny(section, {"compdb_fast", "compdb-fast"}, boolValue,
                                     errorMessage, std::string(location) + ".compdb_fast",
                                     hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_compdb_fast = boolValue;
            }

            if (!readOptionalBoolAny(section, {"quiet"}, boolValue, errorMessage,
                                     std::string(location) + ".quiet", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.quiet = boolValue;
            }

            uint64_t uintValue = 0;
            if (!readOptionalUint64Any(section, {"stack_limit", "stack-limit"}, uintValue,
                                       errorMessage, std::string(location) + ".stack_limit",
                                       hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_limit = uintValue;
            }

            if (!readOptionalScalarAsStringAny(section, {"jobs"}, stringValue, errorMessage,
                                               std::string(location) + ".jobs", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                if (!validateJobsValue(stringValue, std::string(location) + ".jobs", errorMessage))
                {
                    return false;
                }
                config.global.stack_analyzer_jobs = trimCopy(stringValue);
            }

            if (!readOptionalStringAny(section, {"analysis-profile", "analysis_profile"},
                                       stringValue, errorMessage,
                                       std::string(location) + ".analysis_profile", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                if (!validateAnalysisProfile(
                        stringValue, std::string(location) + ".analysis_profile", errorMessage))
                {
                    return false;
                }
                config.global.analysis_profile = stringValue;
            }

            bool smtBool = false;
            if (!readOptionalBoolLikeAny(section, {"smt"}, smtBool, errorMessage,
                                         std::string(location) + ".smt", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.smt = smtBool ? "on" : "off";
            }

            if (!readOptionalStringAny(section, {"smt-backend", "smt_backend"}, stringValue,
                                       errorMessage, std::string(location) + ".smt_backend",
                                       hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.smt_backend = stringValue;
            }

            if (!readOptionalStringAny(section, {"smt-secondary-backend", "smt_secondary_backend"},
                                       stringValue, errorMessage,
                                       std::string(location) + ".smt_secondary_backend", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.smt_secondary_backend = stringValue;
            }

            if (!readOptionalStringAny(section, {"smt-mode", "smt_mode"}, stringValue, errorMessage,
                                       std::string(location) + ".smt_mode", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                if (!validateSmtMode(stringValue, std::string(location) + ".smt_mode",
                                     errorMessage))
                {
                    return false;
                }
                config.global.smt_mode = stringValue;
            }

            if (!readOptionalUint64Any(section, {"smt-timeout-ms", "smt_timeout_ms"}, uintValue,
                                       errorMessage, std::string(location) + ".smt_timeout_ms",
                                       hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                if (uintValue > std::numeric_limits<uint32_t>::max())
                {
                    errorMessage = std::string(location) + ".smt_timeout_ms is too large.";
                    return false;
                }
                config.global.smt_timeout_ms = static_cast<uint32_t>(uintValue);
            }

            if (!readOptionalUint64Any(section, {"smt-budget-nodes", "smt_budget_nodes"}, uintValue,
                                       errorMessage, std::string(location) + ".smt_budget_nodes",
                                       hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.smt_budget_nodes = uintValue;
            }

            if (!readOptionalStringListAny(section, {"smt-rules", "smt_rules"}, listValue,
                                           errorMessage, std::string(location) + ".smt_rules",
                                           hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.smt_rules = listValue;
            }

            if (!readOptionalStringListAny(section,
                                           {"only_functions", "only-functions", "only_function",
                                            "only-function", "only_func", "only-func"},
                                           listValue, errorMessage,
                                           std::string(location) + ".only_functions", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_only_functions = listValue;
            }

            if (!readOptionalStringListAny(
                    section, {"only_files", "only-files", "only_file", "only-file"}, listValue,
                    errorMessage, std::string(location) + ".only_files", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_only_files = listValue;
            }

            if (!readOptionalStringListAny(
                    section, {"only_dirs", "only-dirs", "only_dir", "only-dir"}, listValue,
                    errorMessage, std::string(location) + ".only_dirs", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_only_dirs = listValue;
            }

            if (!readOptionalStringListAny(
                    section, {"exclude_dirs", "exclude-dirs", "exclude_dir", "exclude-dir"},
                    listValue, errorMessage, std::string(location) + ".exclude_dirs", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_exclude_dirs = listValue;
            }

            if (!readOptionalStringListAny(section, {"entry_points"}, listValue, errorMessage,
                                           std::string(location) + ".entry_points", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                (void)parseEntryPoints(listValue, config);
                config.global.stack_analyzer_only_functions = listValue;
            }

            if (!readOptionalBoolAny(section, {"resource_cross_tu", "resource-cross-tu"}, boolValue,
                                     errorMessage, std::string(location) + ".resource_cross_tu",
                                     hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_resource_cross_tu = boolValue;
            }

            if (!readOptionalBoolAny(section, {"no_resource_cross_tu", "no-resource-cross-tu"},
                                     boolValue, errorMessage,
                                     std::string(location) + ".no_resource_cross_tu", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_resource_cross_tu = !boolValue;
            }

            if (!readOptionalBoolAny(section, {"uninitialized_cross_tu", "uninitialized-cross-tu"},
                                     boolValue, errorMessage,
                                     std::string(location) + ".uninitialized_cross_tu", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_uninitialized_cross_tu = boolValue;
            }

            if (!readOptionalBoolAny(
                    section, {"no_uninitialized_cross_tu", "no-uninitialized-cross-tu"}, boolValue,
                    errorMessage, std::string(location) + ".no_uninitialized_cross_tu", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_uninitialized_cross_tu = !boolValue;
            }

            if (!readOptionalStringAny(
                    section, {"resource_summary_cache_dir", "resource-summary-cache-dir"},
                    stringValue, errorMessage,
                    std::string(location) + ".resource_summary_cache_dir", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_resource_summary_cache_dir =
                    stringValue.empty() ? std::string()
                                        : resolvePathFromBase(configDir, stringValue).string();
            }

            if (!readOptionalBoolAny(
                    section,
                    {"resource_summary_cache_memory_only", "resource-summary-cache-memory-only"},
                    boolValue, errorMessage,
                    std::string(location) + ".resource_summary_cache_memory_only", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_resource_summary_cache_memory_only = boolValue;
            }

            if (!readOptionalStringAny(section, {"compile_ir_cache_dir", "compile-ir-cache-dir"},
                                       stringValue, errorMessage,
                                       std::string(location) + ".compile_ir_cache_dir", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_compile_ir_cache_dir =
                    stringValue.empty() ? std::string()
                                        : resolvePathFromBase(configDir, stringValue).string();
            }

            if (!readOptionalStringAny(section, {"compile_ir_format", "compile-ir-format"},
                                       stringValue, errorMessage,
                                       std::string(location) + ".compile_ir_format", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                if (!validateCompileIRFormat(
                        stringValue, std::string(location) + ".compile_ir_format", errorMessage))
                {
                    return false;
                }
                config.global.stack_analyzer_compile_ir_format = stringValue;
            }

            if (!readOptionalBoolAny(section, {"include_stl", "include-stl", "stl"}, boolValue,
                                     errorMessage, std::string(location) + ".include_stl",
                                     hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_include_stl = boolValue;
            }

            if (!readOptionalStringAny(section, {"base_dir", "base-dir"}, stringValue, errorMessage,
                                       std::string(location) + ".base_dir", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_base_dir =
                    stringValue.empty() ? std::string()
                                        : resolvePathFromBase(configDir, stringValue).string();
            }

            if (!readOptionalBoolAny(section, {"dump_filter", "dump-filter"}, boolValue,
                                     errorMessage, std::string(location) + ".dump_filter",
                                     hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_dump_filter = boolValue;
            }

            if (!readOptionalStringAny(section, {"dump_ir", "dump-ir"}, stringValue, errorMessage,
                                       std::string(location) + ".dump_ir", hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_dump_ir =
                    stringValue.empty() ? std::string()
                                        : resolvePathFromBase(configDir, stringValue).string();
            }

            if (!readOptionalBoolAny(section, {"warnings_only", "warnings-only"}, boolValue,
                                     errorMessage, std::string(location) + ".warnings_only",
                                     hasValue))
            {
                return false;
            }
            if (hasValue)
            {
                config.global.stack_analyzer_warnings_only = boolValue;
            }

            return true;
        }

        [[nodiscard]] const json* findStackAnalyzerSection(const json& root,
                                                           std::string& errorMessage)
        {
            const auto itDirect = root.find("stack_analyzer");
            if (itDirect != root.end() && !itDirect->is_null())
            {
                if (!itDirect->is_object())
                {
                    errorMessage = "Expected object for 'stack_analyzer'.";
                    return nullptr;
                }
                return &(*itDirect);
            }

            const auto itAlias = root.find("stack-analyzer");
            if (itAlias != root.end() && !itAlias->is_null())
            {
                if (!itAlias->is_object())
                {
                    errorMessage = "Expected object for 'stack-analyzer'.";
                    return nullptr;
                }
                return &(*itAlias);
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

            const auto itToolAnalyzer = itTools->find("ctrace_stack_analyzer");
            if (itToolAnalyzer != itTools->end() && !itToolAnalyzer->is_null())
            {
                if (!itToolAnalyzer->is_object())
                {
                    errorMessage = "Expected object for 'tools.ctrace_stack_analyzer'.";
                    return nullptr;
                }
                return &(*itToolAnalyzer);
            }

            const auto itToolLegacy = itTools->find("stack_analyzer");
            if (itToolLegacy != itTools->end() && !itToolLegacy->is_null())
            {
                if (!itToolLegacy->is_object())
                {
                    errorMessage = "Expected object for 'tools.stack_analyzer'.";
                    return nullptr;
                }
                return &(*itToolLegacy);
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

        [[nodiscard]] bool applyCanonicalSections(const json& root,
                                                  const std::filesystem::path& configDir,
                                                  ProgramConfig& config, std::string& errorMessage)
        {
            if (const auto it = root.find("analysis"); it != root.end() && !it->is_null())
            {
                if (!it->is_object())
                {
                    errorMessage = "Expected object for 'analysis'.";
                    return false;
                }
                if (!applyAnalysisSection(*it, config, errorMessage))
                {
                    return false;
                }
            }

            if (const auto it = root.find("files"); it != root.end() && !it->is_null())
            {
                if (!it->is_object())
                {
                    errorMessage = "Expected object for 'files'.";
                    return false;
                }
                if (!applyFilesSection(*it, configDir, config, errorMessage))
                {
                    return false;
                }
            }

            if (const auto it = root.find("output"); it != root.end() && !it->is_null())
            {
                if (!it->is_object())
                {
                    errorMessage = "Expected object for 'output'.";
                    return false;
                }
                if (!applyOutputSection(*it, config, errorMessage))
                {
                    return false;
                }
            }

            if (const auto it = root.find("runtime"); it != root.end() && !it->is_null())
            {
                if (!it->is_object())
                {
                    errorMessage = "Expected object for 'runtime'.";
                    return false;
                }
                if (!applyRuntimeSection(*it, config, errorMessage))
                {
                    return false;
                }
            }

            if (const auto it = root.find("server"); it != root.end() && !it->is_null())
            {
                if (!it->is_object())
                {
                    errorMessage = "Expected object for 'server'.";
                    return false;
                }
                if (!applyServerSection(*it, config, errorMessage))
                {
                    return false;
                }
            }

            return true;
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
                               {
                                   "schema_version",
                                   "analysis",
                                   "files",
                                   "output",
                                   "runtime",
                                   "server",
                                   "stack_analyzer",
                                   "stack-analyzer",
                                   "invoke",
                                   "input",
                                   "tools",
                               },
                               "root", errorMessage))
        {
            return false;
        }

        if (!applySchemaVersion(root, errorMessage))
        {
            return false;
        }

        const std::filesystem::path configDir = std::filesystem::absolute(path).parent_path();
        config.global.config_file = path.lexically_normal().string();

        if (const auto* analyzerSection = findStackAnalyzerSection(root, errorMessage);
            analyzerSection != nullptr)
        {
            if (!applyStackAnalyzerSection(*analyzerSection, configDir, config, errorMessage,
                                           "stack_analyzer"))
            {
                return false;
            }
        }
        else if (!errorMessage.empty())
        {
            return false;
        }

        if (!applyLegacyRootInvokeAndInput(root, configDir, config, errorMessage))
        {
            return false;
        }

        if (!applyCanonicalSections(root, configDir, config, errorMessage))
        {
            return false;
        }

        return true;
    }
} // namespace ctrace
