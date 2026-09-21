// SPDX-License-Identifier: Apache-2.0
// server.cpp
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "httplib.h"         // cpp-httplib (header-only)
#include <nlohmann/json.hpp> // nlohmann::json (header-only)

#include "Config/config.hpp"
#include "App/Files.hpp"
#include "App/ToolConfig.hpp"
#include "Process/Tools/ToolsInvoker.hpp"
#include "ctrace_tools/strings.hpp"
#include "coretrace/logger.hpp"

using json = nlohmann::json;

// ============================================================================
// Logging
// ============================================================================

class ILogger
{
  public:
    virtual ~ILogger() = default;
    virtual void info(const std::string& msg) = 0;
    virtual void error(const std::string& msg) = 0;
};

class ConsoleLogger : public ILogger
{
  public:
    void info(const std::string& msg) override
    {
        coretrace::log(coretrace::Level::Info, "{}\n", msg);
    }

    void error(const std::string& msg) override
    {
        coretrace::log(coretrace::Level::Error, "{}\n", msg);
    }

    void debug(const std::string& msg)
    {
        coretrace::log(coretrace::Level::Debug, "{}\n", msg);
    }

    void warn(const std::string& msg)
    {
        coretrace::log(coretrace::Level::Warn, "{}\n", msg);
    }
};

// ============================================================================
// API / Contrôleur : gestion du protocole JSON-RPC-like
// ============================================================================

class ApiHandler
{
  public:
    explicit ApiHandler(ILogger& logger) : logger_(logger) {}

    json handle_request(const json& request)
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

    struct ParseError
    {
        std::string code;
        std::string message;
    };

    static bool build_config_from_params(const json& params, ctrace::ProgramConfig& config,
                                         ParseError& err);

  private:
    ILogger& logger_;

    static void log_request(ILogger& logger, const json& request)
    {
        logger.info("Incoming request: " + request.dump());
    }

    static bool read_string(const json& params, const char* key, std::string& out, ParseError& err)
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
    static bool read_string_list(const json& value, const char* key, bool splitItems,
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

    static std::string join_with_comma(const std::vector<std::string>& items)
    {
        return ctrace_tools::strings::joinByComma(items);
    }

    /// `ipc` is protocol-specific: "serv"/"server" alias "serve", and "serve" inside a request
    /// means the analysis runs in-process (standardIO).
    static bool apply_ipc_field(const json& params, ctrace::ProgramConfig& config, ParseError& err)
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

    static const std::vector<ParamSpec>& param_schema()
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
    static std::string with_param_names(std::string message)
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
    static bool build_config_from_params_impl(const json& params, ctrace::ProgramConfig& config,
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

    static bool run_analysis(const ctrace::ProgramConfig& config, ILogger& logger, json& result,
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

    json handle_run_analysis(json& baseResponse, const json& params)
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
};

inline bool ApiHandler::build_config_from_params(const json& params, ctrace::ProgramConfig& config,
                                                 ParseError& err)
{
    return build_config_from_params_impl(params, config, err);
}

// ============================================================================
// Couche HTTP / Transport
// ============================================================================

class HttpServer
{
  public:
    HttpServer(ApiHandler& apiHandler, ILogger& logger, const ctrace::ServerConfig& config)
        : apiHandler_(apiHandler), logger_(logger), shutdown_token_(config.shutdown_token),
          cors_origin_(config.cors_origin),
          shutdown_timeout_(std::chrono::milliseconds(config.shutdown_timeout_ms))
    {
        // Requests carry a file list, not file contents; cpp-httplib is otherwise unbounded.
        if (config.max_body_bytes > 0)
        {
            server_.set_payload_max_length(static_cast<size_t>(config.max_body_bytes));
        }
    }

    /// Stops accepting connections. Safe to call from another thread.
    void stop()
    {
        server_.stop();
    }

    void run(const std::string& host, int port)
    {
        // CORS
        server_.Options("/api",
                        [this](const httplib::Request&, httplib::Response& res)
                        {
                            set_cors(res);
                            if (is_shutting_down())
                            {
                                res.status = 503;
                            }
                            else
                            {
                                res.status = 200;
                            }
                        });

        // Endpoint principal
        server_.Post("/api",
                     [this](const httplib::Request& req, httplib::Response& res)
                     {
                         set_cors(res);
                         handle_post_api(req, res);
                     });

        // Shutdown endpoint
        server_.Options("/shutdown",
                        [this](const httplib::Request&, httplib::Response& res)
                        {
                            set_cors(res);
                            if (is_shutting_down())
                            {
                                res.status = 503;
                            }
                            else
                            {
                                res.status = 200;
                            }
                        });

        server_.Post("/shutdown",
                     [this](const httplib::Request& req, httplib::Response& res)
                     {
                         set_cors(res);
                         handle_post_shutdown(req, res);
                     });

        logger_.info("Listening on http://" + host + ":" + std::to_string(port));
        server_.listen(host.c_str(), port);
        finalize_shutdown();
    }

  private:
    struct InFlightGuard
    {
        explicit InFlightGuard(HttpServer& server) : server_(server)
        {
            server_.begin_request();
        }

        ~InFlightGuard()
        {
            server_.end_request();
        }

        HttpServer& server_;
    };

    httplib::Server server_;
    ApiHandler& apiHandler_;
    ILogger& logger_;
    std::atomic<bool> shutting_down_{false};
    std::atomic<bool> shutdown_requested_{false};
    std::atomic<int> in_flight_{0};
    std::mutex shutdown_mutex_;
    std::condition_variable shutdown_cv_;
    std::thread shutdown_thread_;
    std::string shutdown_token_;
    std::string cors_origin_;
    std::chrono::milliseconds shutdown_timeout_{0};

    bool is_shutting_down() const
    {
        return shutting_down_.load(std::memory_order_acquire);
    }

    /// Cross-origin access is opt-in: without a configured origin no header is sent, so a
    /// page on another origin cannot read the response.
    void set_cors(httplib::Response& res) const
    {
        if (cors_origin_.empty())
        {
            return;
        }
        res.set_header("Access-Control-Allow-Origin", cors_origin_);
        res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    }

    void begin_request()
    {
        in_flight_.fetch_add(1, std::memory_order_relaxed);
    }

    void end_request()
    {
        const int remaining = in_flight_.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (remaining == 0)
        {
            shutdown_cv_.notify_all();
        }
    }

    static void flush_logs()
    {
        std::cout << std::flush;
        std::cerr << std::flush;
    }

    bool is_authorized_shutdown(const httplib::Request& req) const
    {
        if (shutdown_token_.empty())
        {
            return false;
        }

        const std::string bearer = req.get_header_value("Authorization");
        if (!bearer.empty())
        {
            const std::string prefix = "Bearer ";
            if (bearer.rfind(prefix, 0) == 0)
            {
                return bearer.substr(prefix.size()) == shutdown_token_;
            }
            return bearer == shutdown_token_;
        }

        const std::string admin_token = req.get_header_value("X-Admin-Token");
        return !admin_token.empty() && admin_token == shutdown_token_;
    }

    void initiate_shutdown()
    {
        bool expected = false;
        if (!shutdown_requested_.compare_exchange_strong(expected, true))
        {
            return;
        }

        shutting_down_.store(true, std::memory_order_release);

        shutdown_thread_ = std::thread(
            [this]()
            {
                logger_.info("[SERVER] Shutdown requested. Stopping listener...");
                server_.stop();
                wait_for_inflight_or_timeout();
            });
    }

    void wait_for_inflight_or_timeout()
    {
        std::unique_lock<std::mutex> lock(shutdown_mutex_);
        const auto done = [this]() { return in_flight_.load(std::memory_order_acquire) == 0; };

        if (shutdown_timeout_.count() > 0)
        {
            if (!shutdown_cv_.wait_for(lock, shutdown_timeout_, done))
            {
                logger_.error("[SERVER] Shutdown timeout exceeded. Forcing exit.");
            }
        }
        else
        {
            shutdown_cv_.wait(lock, done);
        }
    }

    void finalize_shutdown()
    {
        if (shutdown_thread_.joinable())
        {
            shutdown_thread_.join();
        }
        if (shutdown_requested_.load(std::memory_order_acquire))
        {
            logger_.info("[SERVER] Shutdown complete.");
        }
        flush_logs();
    }

    void handle_post_api(const httplib::Request& req, httplib::Response& res)
    {
        if (is_shutting_down())
        {
            json err;
            err["proto"] = "coretrace-1.0";
            err["type"] = "response";
            err["status"] = "error";
            err["error"] = {{"code", "ServerShuttingDown"},
                            {"message", "Server is shutting down."}};
            res.status = 503;
            res.set_content(err.dump(), "application/json");
            return;
        }

        InFlightGuard guard(*this);
        try
        {
            json request = json::parse(req.body);
            json response = apiHandler_.handle_request(request);

            res.status = 200;
            res.set_content(response.dump(), "application/json");
        }
        catch (const std::exception& e)
        {
            logger_.error(std::string("Exception while handling /api: ") + e.what());

            json err;
            err["proto"] = "coretrace-1.0";
            err["type"] = "response";
            err["status"] = "error";
            err["error"] = {{"code", "InvalidRequest"}, {"message", e.what()}};

            res.status = 400;
            res.set_content(err.dump(), "application/json");
        }
    }

    void handle_post_shutdown(const httplib::Request& req, httplib::Response& res)
    {
        if (!is_authorized_shutdown(req))
        {
            json err;
            err["status"] = "error";
            err["error"] = {{"code", "Unauthorized"},
                            {"message", shutdown_token_.empty() ? "Shutdown token not configured."
                                                                : "Invalid shutdown token."}};
            res.status = 403;
            res.set_content(err.dump(), "application/json");
            return;
        }

        if (shutdown_requested_.load(std::memory_order_acquire))
        {
            json ok;
            ok["status"] = "accepted";
            ok["message"] = "Shutdown already in progress.";
            ok["timeout_ms"] = shutdown_timeout_.count();
            res.status = 202;
            res.set_content(ok.dump(), "application/json");
            return;
        }

        json ok;
        ok["status"] = "accepted";
        ok["message"] = "Shutdown initiated.";
        ok["timeout_ms"] = shutdown_timeout_.count();
        res.status = 202;
        res.set_content(ok.dump(), "application/json");

        initiate_shutdown();
    }
};
