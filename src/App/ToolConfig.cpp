#include "App/ToolConfig.hpp"

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
#include <vector>

namespace ctrace
{
    namespace
    {
        using json = nlohmann::json;

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

        [[nodiscard]] bool readStringValue(const json& object, const char* key, std::string& out,
                                           std::string& errorMessage)
        {
            const auto it = object.find(key);
            if (it == object.end() || it->is_null())
            {
                return true;
            }
            if (!it->is_string())
            {
                errorMessage = std::string("Expected string for '") + key + "'.";
                return false;
            }
            out = it->get<std::string>();
            return true;
        }

        [[nodiscard]] bool readBoolValue(const json& object, const char* key, bool& out,
                                         std::string& errorMessage)
        {
            const auto it = object.find(key);
            if (it == object.end() || it->is_null())
            {
                return true;
            }
            if (!it->is_boolean())
            {
                errorMessage = std::string("Expected boolean for '") + key + "'.";
                return false;
            }
            out = it->get<bool>();
            return true;
        }

        [[nodiscard]] bool readUint64Value(const json& object, const char* key, uint64_t& out,
                                           std::string& errorMessage)
        {
            const auto it = object.find(key);
            if (it == object.end() || it->is_null())
            {
                return true;
            }
            if (!it->is_number_unsigned())
            {
                errorMessage = std::string("Expected unsigned integer for '") + key + "'.";
                return false;
            }
            out = it->get<uint64_t>();
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

        [[nodiscard]] bool readStringValueAny(const json& object,
                                              std::initializer_list<const char*> keys,
                                              std::string& out, std::string& errorMessage)
        {
            const json* value = findFirstValueForKeys(object, keys);
            if (value == nullptr || value->is_null())
            {
                return true;
            }
            if (!value->is_string())
            {
                errorMessage = "Expected string value in stack_analyzer config.";
                return false;
            }
            out = value->get<std::string>();
            return true;
        }

        [[nodiscard]] bool parseBoolLikeString(const std::string& raw, bool& out)
        {
            std::string lowered;
            lowered.reserve(raw.size());
            for (const auto ch : raw)
            {
                lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            }

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
            return false;
        }

        [[nodiscard]] bool readBoolLikeValueAny(const json& object,
                                                std::initializer_list<const char*> keys, bool& out,
                                                std::string& errorMessage)
        {
            const json* value = findFirstValueForKeys(object, keys);
            if (value == nullptr || value->is_null())
            {
                return true;
            }
            if (value->is_boolean())
            {
                out = value->get<bool>();
                return true;
            }
            if (value->is_string())
            {
                const auto raw = value->get<std::string>();
                if (parseBoolLikeString(raw, out))
                {
                    return true;
                }
            }
            errorMessage = "Expected boolean or boolean-like string in stack_analyzer config.";
            return false;
        }

        [[nodiscard]] bool readUint64ValueAny(const json& object,
                                              std::initializer_list<const char*> keys,
                                              uint64_t& out, std::string& errorMessage)
        {
            const json* value = findFirstValueForKeys(object, keys);
            if (value == nullptr || value->is_null())
            {
                return true;
            }
            if (!value->is_number_unsigned())
            {
                errorMessage = "Expected unsigned integer value in stack_analyzer config.";
                return false;
            }
            out = value->get<uint64_t>();
            return true;
        }

        [[nodiscard]] bool parseStringList(const json& value, std::vector<std::string>& out,
                                           std::string& errorMessage, const char* keyName)
        {
            out.clear();
            if (value.is_string())
            {
                out.push_back(value.get<std::string>());
                return true;
            }
            if (!value.is_array())
            {
                errorMessage =
                    std::string("Expected string or array of strings for '") + keyName + "'.";
                return false;
            }
            out.reserve(value.size());
            for (const auto& item : value)
            {
                if (!item.is_string())
                {
                    errorMessage = std::string("Expected string entries in '") + keyName + "'.";
                    return false;
                }
                out.push_back(item.get<std::string>());
            }
            return true;
        }

        [[nodiscard]] bool readStringListValueAny(const json& object,
                                                  std::initializer_list<const char*> keys,
                                                  std::vector<std::string>& out,
                                                  std::string& errorMessage, const char* keyName)
        {
            const json* value = findFirstValueForKeys(object, keys);
            if (value == nullptr || value->is_null())
            {
                return true;
            }
            return parseStringList(*value, out, errorMessage, keyName);
        }

        void appendUnique(std::vector<std::string>& target, const std::string& value)
        {
            if (value.empty())
            {
                return;
            }
            if (std::find(target.begin(), target.end(), value) == target.end())
            {
                target.push_back(value);
            }
        }

        [[nodiscard]] std::string joinComma(const std::vector<std::string>& values)
        {
            std::string joined;
            for (std::size_t i = 0; i < values.size(); ++i)
            {
                if (i != 0)
                {
                    joined += ",";
                }
                joined += values[i];
            }
            return joined;
        }

        [[nodiscard]] const json* findStackAnalyzerConfigSection(const json& root)
        {
            if (const auto it = root.find("stack_analyzer"); it != root.end() && it->is_object())
            {
                return &(*it);
            }
            if (const auto it = root.find("tools"); it != root.end() && it->is_object())
            {
                if (const auto itTool = it->find("ctrace_stack_analyzer");
                    itTool != it->end() && itTool->is_object())
                {
                    return &(*itTool);
                }
                if (const auto itTool = it->find("stack_analyzer");
                    itTool != it->end() && itTool->is_object())
                {
                    return &(*itTool);
                }
            }
            return nullptr;
        }

        [[nodiscard]] bool applyInvoke(const json& root, ProgramConfig& config,
                                       std::string& errorMessage)
        {
            const auto it = root.find("invoke");
            if (it == root.end() || it->is_null())
            {
                return true;
            }

            std::vector<std::string> tools;
            if (!parseStringList(*it, tools, errorMessage, "invoke"))
            {
                return false;
            }

            if (!tools.empty())
            {
                config.global.hasInvokedSpecificTools = true;
                for (const auto& tool : tools)
                {
                    appendUnique(config.global.specificTools, tool);
                }
            }
            return true;
        }

        [[nodiscard]] bool applyInputFiles(const json& root, const std::filesystem::path& configDir,
                                           ProgramConfig& config, std::string& errorMessage)
        {
            const auto it = root.find("input");
            if (it == root.end() || it->is_null())
            {
                return true;
            }

            std::vector<std::string> entries;
            if (!parseStringList(*it, entries, errorMessage, "input"))
            {
                return false;
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
            return true;
        }

        [[nodiscard]] bool applyStackAnalyzerConfig(const json& section,
                                                    const std::filesystem::path& configDir,
                                                    ProgramConfig& config,
                                                    std::string& errorMessage)
        {
            if (!section.is_object())
            {
                errorMessage = "stack_analyzer config must be a JSON object.";
                return false;
            }

            std::string pathValue;
            if (!readStringValue(section, "compile_commands", pathValue, errorMessage))
            {
                return false;
            }
            if (!pathValue.empty())
            {
                config.global.compile_commands = resolvePathFromBase(configDir, pathValue).string();
            }

            pathValue.clear();
            if (!readStringValue(section, "resource_model", pathValue, errorMessage))
            {
                return false;
            }
            if (!pathValue.empty())
            {
                config.global.resource_model = resolvePathFromBase(configDir, pathValue).string();
            }

            pathValue.clear();
            if (!readStringValue(section, "escape_model", pathValue, errorMessage))
            {
                return false;
            }
            if (!pathValue.empty())
            {
                config.global.escape_model = resolvePathFromBase(configDir, pathValue).string();
            }

            pathValue.clear();
            if (!readStringValue(section, "buffer_model", pathValue, errorMessage))
            {
                return false;
            }
            if (!pathValue.empty())
            {
                config.global.buffer_model = resolvePathFromBase(configDir, pathValue).string();
            }

            if (!readBoolValue(section, "demangle", config.global.demangle, errorMessage))
            {
                return false;
            }
            if (!readBoolValue(section, "timing", config.global.timing, errorMessage))
            {
                return false;
            }
            if (!readBoolValue(section, "include_compdb_deps", config.global.include_compdb_deps,
                               errorMessage))
            {
                return false;
            }
            if (!readBoolValue(section, "quiet", config.global.quiet, errorMessage))
            {
                return false;
            }
            if (!readUint64Value(section, "stack_limit", config.global.stack_limit, errorMessage))
            {
                return false;
            }

            std::string stringValue;
            if (!readStringValueAny(section, {"analysis-profile", "analysis_profile"}, stringValue,
                                    errorMessage))
            {
                return false;
            }
            if (!stringValue.empty())
            {
                config.global.analysis_profile = stringValue;
            }

            bool boolValue = false;
            if (!readBoolLikeValueAny(section, {"smt"}, boolValue, errorMessage))
            {
                return false;
            }
            if (const auto* smtValue = findFirstValueForKeys(section, {"smt"});
                smtValue != nullptr && !smtValue->is_null())
            {
                config.global.smt = boolValue ? "on" : "off";
            }

            stringValue.clear();
            if (!readStringValueAny(section, {"smt-backend", "smt_backend"}, stringValue,
                                    errorMessage))
            {
                return false;
            }
            if (!stringValue.empty())
            {
                config.global.smt_backend = stringValue;
            }

            stringValue.clear();
            if (!readStringValueAny(section, {"smt-secondary-backend", "smt_secondary_backend"},
                                    stringValue, errorMessage))
            {
                return false;
            }
            if (!stringValue.empty())
            {
                config.global.smt_secondary_backend = stringValue;
            }

            stringValue.clear();
            if (!readStringValueAny(section, {"smt-mode", "smt_mode"}, stringValue, errorMessage))
            {
                return false;
            }
            if (!stringValue.empty())
            {
                config.global.smt_mode = stringValue;
            }

            uint64_t uint64Value = 0;
            if (!readUint64ValueAny(section, {"smt-timeout-ms", "smt_timeout_ms"}, uint64Value,
                                    errorMessage))
            {
                return false;
            }
            if (const auto* timeoutValue =
                    findFirstValueForKeys(section, {"smt-timeout-ms", "smt_timeout_ms"});
                timeoutValue != nullptr && !timeoutValue->is_null())
            {
                if (uint64Value > std::numeric_limits<uint32_t>::max())
                {
                    errorMessage = "smt-timeout-ms is too large.";
                    return false;
                }
                config.global.smt_timeout_ms = static_cast<uint32_t>(uint64Value);
            }

            uint64Value = 0;
            if (!readUint64ValueAny(section, {"smt-budget-nodes", "smt_budget_nodes"}, uint64Value,
                                    errorMessage))
            {
                return false;
            }
            if (const auto* budgetValue =
                    findFirstValueForKeys(section, {"smt-budget-nodes", "smt_budget_nodes"});
                budgetValue != nullptr && !budgetValue->is_null())
            {
                config.global.smt_budget_nodes = uint64Value;
            }

            std::vector<std::string> rules;
            if (!readStringListValueAny(section, {"smt-rules", "smt_rules"}, rules, errorMessage,
                                        "smt-rules"))
            {
                return false;
            }
            if (!rules.empty() ||
                (findFirstValueForKeys(section, {"smt-rules", "smt_rules"}) != nullptr))
            {
                config.global.smt_rules = std::move(rules);
            }

            if (const auto itEntry = section.find("entry_points");
                itEntry != section.end() && !itEntry->is_null())
            {
                std::vector<std::string> points;
                if (!parseStringList(*itEntry, points, errorMessage, "entry_points"))
                {
                    return false;
                }
                config.global.entry_points = joinComma(points);
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

        const std::filesystem::path configDir = path.parent_path();
        config.global.config_file = path.lexically_normal().string();

        if (!applyInvoke(root, config, errorMessage))
        {
            return false;
        }
        if (!applyInputFiles(root, configDir, config, errorMessage))
        {
            return false;
        }

        if (const auto* analyzerSection = findStackAnalyzerConfigSection(root);
            analyzerSection != nullptr)
        {
            if (!applyStackAnalyzerConfig(*analyzerSection, configDir, config, errorMessage))
            {
                return false;
            }
        }

        return true;
    }
} // namespace ctrace
