// SPDX-License-Identifier: Apache-2.0
#include "Process/Ipc/ApiHandler.hpp"

#include "App/Files.hpp"
#include "App/ToolConfig.hpp"
#include "Process/Tools/ToolsInvoker.hpp"
#include "ctrace_tools/strings.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using json = nlohmann::json;
using ParseError = ApiHandler::ParseError;

namespace
{
    void log_request(ILogger& logger, const json& request)
    {
        logger.info("Incoming request: " + request.dump());
    }

    bool read_string(const json& params, const char* key, std::string& out, ParseError& err)
    {
        const auto it = params.find(key);
        if (it == params.end() || it->is_null())
        {
            return true;
        }
        if (!it->is_string())
        {
            err = {"InvalidParams", std::string("Expected string for '") + key + "'."};
            return false;
        }
        out = it->get<std::string>();
        return true;
    }

    /// List parameters accept a comma-separated string or an array of strings; empty items are
    /// dropped. `input` items are themselves comma-separated (historical CLI form).
    bool read_string_list(const json& value, const char* key, bool splitItems,
                          std::vector<std::string>& out, ParseError& err)
    {
        const auto append = [&](const std::string& item)
        {
            if (!splitItems)
            {
                if (!item.empty())
                {
                    out.push_back(item);
                }
                return;
            }
            for (const auto part : ctrace_tools::strings::splitByComma(item))
            {
                if (!part.empty())
                {
                    out.emplace_back(part);
                }
            }
        };

        if (value.is_string())
        {
            const std::string raw = value.get<std::string>();
            for (const auto part : ctrace_tools::strings::splitByComma(raw))
            {
                append(std::string(part));
            }
            return true;
        }
        if (!value.is_array())
        {
            err = {"InvalidParams", std::string("Expected array or string for '") + key + "'."};
            return false;
        }
        for (const auto& item : value)
        {
            if (!item.is_string())
            {
                err = {"InvalidParams",
                       std::string("Expected string values in '") + key + "' array."};
                return false;
            }
            append(item.get<std::string>());
        }
        return true;
    }

    std::string join_with_comma(const std::vector<std::string>& items)
    {
        return ctrace_tools::strings::joinByComma(items);
    }

    /// `ipc` is protocol-specific: "serv"/"server" alias "serve", and "serve" inside a request
    /// means the analysis runs in-process (standardIO).
    bool apply_ipc_field(const json& params, ctrace::ProgramConfig& config, ParseError& err)
    {
        std::string ipc_value;
        if (!read_string(params, "ipc", ipc_value, err))
        {
            return false;
        }
        if (ipc_value.empty())
        {
            return true;
        }
        if (ipc_value == "serv" || ipc_value == "server")
        {
            ipc_value = "serve";
        }

        const auto& ipc_list = ctrace_defs::IPC_TYPES;
        if (std::find(ipc_list.begin(), ipc_list.end(), ipc_value) == ipc_list.end())
        {
            err = {"InvalidParams", "Invalid IPC type: '" + ipc_value +
                                        "'. Available IPC types: [" + join_with_comma(ipc_list) +
                                        "]"};
            return false;
        }
        config.runtime.ipc = ipc_value == "serve" ? "standardIO" : ipc_value;
        return true;
    }

    /// Flat run_analysis parameter -> section and key of the config schema.
    struct ParamSpec
    {
        const char* param;
        const char* section;
        const char* key;
        bool list;
    };

    const std::vector<ParamSpec>& param_schema()
    {
        static const std::vector<ParamSpec> schema = {
            {"verbose", "output", "verbose", false},
            {"quiet", "output", "quiet", false},
            {"demangle", "output", "demangle", false},
            {"sarif_format", "output", "sarif_format", false},
            {"report_file", "output", "report_file", false},
            {"output_file", "output", "output_file", false},
            {"static_analysis", "analysis", "static", false},
            {"dynamic_analysis", "analysis", "dynamic", false},
            {"invoke", "analysis", "invoke", true},
            {"input", "files", "input", true},
            {"entry_points", "files", "entry_points", true},
            {"compile_commands", "files", "compile_commands", false},
            {"include_compdb_deps", "files", "include_compdb_deps", false},
            {"async", "runtime", "async", false},
            {"ipc_path", "runtime", "ipc_path", false},
            {"timing", "stack_analyzer", "timing", false},
            {"analysis_profile", "stack_analyzer", "analysis_profile", false},
            {"smt", "stack_analyzer", "smt", false},
            {"smt_backend", "stack_analyzer", "smt_backend", false},
            {"smt_secondary_backend", "stack_analyzer", "smt_secondary_backend", false},
            {"smt_mode", "stack_analyzer", "smt_mode", false},
            {"smt_timeout_ms", "stack_analyzer", "smt_timeout_ms", false},
            {"smt_budget_nodes", "stack_analyzer", "smt_budget_nodes", false},
            {"smt_rules", "stack_analyzer", "smt_rules", true},
            {"stack_limit", "stack_analyzer", "stack_limit", false},
            {"resource_model", "stack_analyzer", "resource_model", false},
            {"escape_model", "stack_analyzer", "escape_model", false},
            {"buffer_model", "stack_analyzer", "buffer_model", false},
            {"stack_analyzer_mode", "stack_analyzer", "mode", false},
            {"stack_analyzer_output_format", "stack_analyzer", "output_format", false},
            {"stack_analyzer_extra_args", "stack_analyzer", "extra_args", true},
        };
        return schema;
    }

    /// Loader diagnostics name the sectioned key; clients know the flat parameter.
    std::string with_param_names(std::string message)
    {
        std::vector<std::pair<std::string, std::string>> renames;
        for (const ParamSpec& spec : param_schema())
        {
            renames.emplace_back(std::string(spec.section) + "." + spec.key, spec.param);
        }
        std::sort(renames.begin(), renames.end(),
                  [](const auto& a, const auto& b) { return a.first.size() > b.first.size(); });
        for (const auto& [path, param] : renames)
        {
            for (auto pos = message.find(path); pos != std::string::npos;
                 pos = message.find(path, pos + param.size()))
            {
                message.replace(pos, path.size(), param);
            }
        }
        return message;
    }

    /// Translates the flat request parameters into the sectioned config document and applies
    /// it through the config loader: one validation path for the file, the request and the CLI.
    bool build_config_from_params_impl(const json& params, ctrace::ProgramConfig& config,
                                       ParseError& err)
    {
        if (!params.is_object())
        {
            err = {"InvalidParams", "Params must be a JSON object."};
            return false;
        }

        // Precedence: defaults < config file < request parameters.
        std::string configPath;
        if (!read_string(params, "config", configPath, err))
        {
            return false;
        }
        if (!configPath.empty())
        {
            std::string toolConfigError;
            if (!ctrace::applyToolConfigFile(config, configPath, toolConfigError))
            {
                err = {"InvalidParams", "Failed to load config: " + toolConfigError};
                return false;
            }
            config.config_file = configPath;
        }

        json document = json::object();
        for (const ParamSpec& spec : param_schema())
        {
            const auto it = params.find(spec.param);
            if (it == params.end() || it->is_null())
            {
                continue;
            }
            if (spec.list)
            {
                std::vector<std::string> items;
                if (!read_string_list(*it, spec.param, std::string(spec.param) == "input", items,
                                      err))
                {
                    return false;
                }
                document[spec.section][spec.key] = items;
                continue;
            }
            document[spec.section][spec.key] = *it;
        }

        std::string loaderError;
        if (!ctrace::applyToolConfigObject(config, document, {}, loaderError))
        {
            err = {"InvalidParams", with_param_names(loaderError)};
            return false;
        }

        return apply_ipc_field(params, config, err);
    }

    bool run_analysis(const ctrace::ProgramConfig& config, ILogger& logger, json& result,
                      ParseError& err)
    {
        if (!config.analysis.static_enabled && !config.analysis.dynamic_enabled &&
            config.analysis.invoke.empty())
        {
            err = {"NoAnalysisSelected",
                   "Enable static_analysis, dynamic_analysis, or invoke tools."};
            return false;
        }

        unsigned int threads = std::thread::hardware_concurrency();
        if (threads == 0)
        {
            threads = 1;
        }
        if (threads > 255)
        {
            threads = 255;
        }
        const uint8_t pool_size = static_cast<uint8_t>(threads);
        auto output_capture = std::make_shared<ctrace::CaptureBuffer>();
        ctrace::ToolInvoker invoker(
            config, pool_size, (config.runtime.async ? std::launch::async : std::launch::deferred),
            output_capture);
        const ctrace::SourceFileResolution resolution = ctrace::resolveSourceFiles(config);
        if (!resolution.ok())
        {
            err = {"InvalidInput", resolution.error};
            return false;
        }
        const std::vector<std::string>& sourceFiles = resolution.files;

        if (config.output.verbose)
        {
            if (!config.config_file.empty())
            {
                logger.info("Config file in use: " + config.config_file);
            }
            else
            {
                logger.info("Config file in use: none (request values only)");
            }
        }

        std::vector<std::string> validSourceFiles;
        validSourceFiles.reserve(sourceFiles.size());
        for (const auto& file : sourceFiles)
        {
            if (!file.empty())
            {
                validSourceFiles.push_back(file);
            }
        }

        const size_t processed = validSourceFiles.size();
        if (processed > 0)
        {
            if (config.analysis.static_enabled)
            {
                invoker.runStaticTools(validSourceFiles);
            }
            if (config.analysis.dynamic_enabled)
            {
                invoker.runDynamicTools(validSourceFiles);
            }
            if (!config.analysis.invoke.empty())
            {
                invoker.runSpecificTools(config.analysis.invoke, validSourceFiles);
            }
        }

        if (processed == 0)
        {
            err = {"MissingInput",
                   "Input files are required for analysis (or provide --compile-commands)."};
            return false;
        }

        logger.info("Analysis completed for " + std::to_string(processed) + " file(s).");

        result["files"] = processed;
        result["static_analysis"] = config.analysis.static_enabled;
        result["dynamic_analysis"] = config.analysis.dynamic_enabled;
        result["invoked_tools"] = config.analysis.invoke;
        result["sarif_format"] = config.output.sarif_format;
        result["report_file"] = config.output.report_file;
        result["config"] = config.config_file;
        result["include_compdb_deps"] = config.files.include_compdb_deps;
        result["resource_model"] = config.stack_analyzer.resource_model;
        result["escape_model"] = config.stack_analyzer.escape_model;
        result["buffer_model"] = config.stack_analyzer.buffer_model;
        result["analysis_profile"] = config.stack_analyzer.analysis_profile;
        result["smt"] = config.stack_analyzer.smt;
        result["smt_backend"] = config.stack_analyzer.smt_backend;
        result["smt_secondary_backend"] = config.stack_analyzer.smt_secondary_backend;
        result["smt_mode"] = config.stack_analyzer.smt_mode;
        result["smt_timeout_ms"] = config.stack_analyzer.smt_timeout_ms;
        result["smt_budget_nodes"] = config.stack_analyzer.smt_budget_nodes;
        result["smt_rules"] = config.stack_analyzer.smt_rules;
        result["timing"] = config.stack_analyzer.timing;
        result["stack_limit"] = config.stack_analyzer.stack_limit;
        result["stack_analyzer_mode"] = config.stack_analyzer.mode;
        result["stack_analyzer_output_format"] = config.stack_analyzer.output_format;
        result["stack_analyzer_extra_args"] = config.stack_analyzer.extra_args;
        const auto diagnosticsSummaryTotal = invoker.diagnosticsSummaryTotal();
        result["diagnostics_summary_total"] = {{"info", diagnosticsSummaryTotal.info},
                                               {"warning", diagnosticsSummaryTotal.warning},
                                               {"error", diagnosticsSummaryTotal.error}};
        if (output_capture)
        {
            json outputs = json::object();
            const auto snapshot = output_capture->snapshot();
            for (const auto& [tool, lines] : snapshot)
            {
                json entries = json::array();
                for (const auto& line : lines)
                {
                    json entry;
                    entry["stream"] = line.stream;

                    const auto& message = line.message;
                    const auto first_non_space = message.find_first_not_of(" \t\n\r");
                    if (first_non_space != std::string::npos &&
                        (message[first_non_space] == '{' || message[first_non_space] == '['))
                    {
                        json parsed = json::parse(message, nullptr, false);
                        if (!parsed.is_discarded())
                        {
                            entry["message"] = parsed;
                        }
                        else
                        {
                            entry["message"] = message;
                        }
                    }
                    else
                    {
                        entry["message"] = message;
                    }

                    entries.push_back(entry);
                }
                outputs[tool] = entries;
            }
            result["outputs"] = outputs;
        }
        return true;
    }

} // namespace

json ApiHandler::handle_request(const json& request)
{
    log_request(logger_, request);

    json response;
    response["proto"] = request.value("proto", std::string("coretrace-1.0"));
    response["id"] = request.value("id", 0);
    response["type"] = "response";

    std::string method = request.value("method", "");
    json params = request.value("params", json::object());

    if (method == "run_analysis")
    {
        return handle_run_analysis(response, params);
    }

    // Méthode inconnue
    response["status"] = "error";
    response["error"] = {{"code", "UnknownMethod"}, {"message", "Unknown method: " + method}};
    return response;
}

bool ApiHandler::build_config_from_params(const json& params, ctrace::ProgramConfig& config,
                                          ParseError& err)
{
    return build_config_from_params_impl(params, config, err);
}

json ApiHandler::handle_run_analysis(json& baseResponse, const json& params)
{
    ctrace::ProgramConfig config;
    ParseError err;

    if (!build_config_from_params(params, config, err))
    {
        baseResponse["status"] = "error";
        baseResponse["error"] = {{"code", err.code}, {"message", err.message}};
        return baseResponse;
    }

    json result;
    if (!run_analysis(config, logger_, result, err))
    {
        baseResponse["status"] = "error";
        baseResponse["error"] = {{"code", err.code}, {"message", err.message}};
        return baseResponse;
    }

    baseResponse["status"] = "ok";
    baseResponse["result"] = result;
    return baseResponse;
}
