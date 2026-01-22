// server.cpp
#include <algorithm>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "httplib.h"              // cpp-httplib (header-only)
#include <nlohmann/json.hpp>      // nlohmann::json (header-only)

#include "Config/config.hpp"
#include "Process/Tools/ToolsInvoker.hpp"
#include "ctrace_tools/strings.hpp"

using json = nlohmann::json;

// ============================================================================
// Logging
// ============================================================================

class ILogger
{
    public:
        virtual ~ILogger() = default;
        virtual void info(const std::string& msg)  = 0;
        virtual void error(const std::string& msg) = 0;
};

class ConsoleLogger : public ILogger
{
    public:
        void info(const std::string& msg) override
        {
            std::cout << "[INFO] :: " << msg << '\n';
        }

        void error(const std::string& msg) override
        {
            std::cerr << "[ERROR] :: " << msg << '\n';
        }

        void debug(const std::string& msg)
        {
            std::cout << "[DEBUG] :: " << msg << '\n';
        }

        void warn(const std::string& msg)
        {
            std::cout << "[WARN] :: " << msg << '\n';
        }
};

// ============================================================================
// API / Contrôleur : gestion du protocole JSON-RPC-like
// ============================================================================

class ApiHandler
{
public:
    explicit ApiHandler(ILogger& logger)
        : logger_(logger)
    {}

    json handle_request(const json& request)
    {
        log_request(logger_, request);

        json response;
        response["proto"] = request.value("proto", std::string("coretrace-1.0"));
        response["id"]    = request.value("id", 0);
        response["type"]  = "response";

        std::string method = request.value("method", "");
        json params        = request.value("params", json::object());

        if (method == "run_analysis") {
            return handle_run_analysis(response, params);
        }

        // Méthode inconnue
        response["status"] = "error";
        response["error"] = {
            {"code", "UnknownMethod"},
            {"message", "Unknown method: " + method}
        };
        return response;
    }

private:
    struct ParseError
    {
        std::string code;
        std::string message;
    };

    struct BoolField
    {
        const char* key;
        bool* target;
    };

    struct StringField
    {
        const char* key;
        std::string* target;
    };

    ILogger& logger_;

    static void log_request(ILogger& logger, const json& request)
    {
        logger.info("Incoming request: " + request.dump());
    }

    static bool read_bool(const json& params, const char* key, bool& out, ParseError& err)
    {
        const auto it = params.find(key);
        if (it == params.end() || it->is_null())
        {
            return true;
        }
        if (!it->is_boolean())
        {
            err = {"InvalidParams", std::string("Expected boolean for '") + key + "'."};
            return false;
        }
        out = it->get<bool>();
        return true;
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

    static bool read_string_list(const json& params, const char* key, std::vector<std::string>& out, ParseError& err)
    {
        const auto it = params.find(key);
        if (it == params.end() || it->is_null())
        {
            return true;
        }

        if (it->is_string())
        {
            const std::string raw = it->get<std::string>();
            const auto parts = ctrace_tools::strings::splitByComma(raw);
            for (const auto part : parts)
            {
                if (!part.empty())
                {
                    out.emplace_back(part);
                }
            }
            return true;
        }

        if (!it->is_array())
        {
            err = {"InvalidParams", std::string("Expected array or string for '") + key + "'."};
            return false;
        }

        for (const auto& item : *it)
        {
            if (!item.is_string())
            {
                err = {"InvalidParams", std::string("Expected string values in '") + key + "' array."};
                return false;
            }
            const std::string value = item.get<std::string>();
            if (!value.empty())
            {
                out.emplace_back(value);
            }
        }
        return true;
    }

    static bool apply_bool_fields(const json& params, ParseError& err, std::initializer_list<BoolField> fields)
    {
        for (const auto& field : fields)
        {
            if (!read_bool(params, field.key, *field.target, err))
            {
                return false;
            }
        }
        return true;
    }

    static bool apply_string_fields(const json& params, ParseError& err, std::initializer_list<StringField> fields)
    {
        for (const auto& field : fields)
        {
            if (!read_string(params, field.key, *field.target, err))
            {
                return false;
            }
        }
        return true;
    }

    template <typename ApplyFn>
    static bool apply_list_param(const json& params, const char* key, ParseError& err, ApplyFn&& apply)
    {
        std::vector<std::string> values;
        if (!read_string_list(params, key, values, err))
        {
            return false;
        }
        if (!values.empty())
        {
            apply(values);
        }
        return true;
    }

    static std::string join_with_comma(const std::vector<std::string>& items)
    {
        std::string joined;
        for (size_t i = 0; i < items.size(); ++i)
        {
            if (i > 0)
            {
                joined.push_back(',');
            }
            joined.append(items[i]);
        }
        return joined;
    }

    static bool apply_async_field(const json& params, ctrace::ProgramConfig& config, ParseError& err)
    {
        bool async_enabled = false;
        if (!read_bool(params, "async", async_enabled, err))
        {
            return false;
        }
        config.global.hasAsync = async_enabled ? std::launch::async : std::launch::deferred;
        return true;
    }

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
            err = {
                "InvalidParams",
                "Invalid IPC type: '" + ipc_value + "'. Available IPC types: [" + join_with_comma(ipc_list) + "]"
            };
            return false;
        }
        if (ipc_value == "serve")
        {
            config.global.ipc = "standardIO";
            return true;
        }

        config.global.ipc = ipc_value;
        return true;
    }

    static bool build_config_from_params(const json& params, ctrace::ProgramConfig& config, ParseError& err)
    {
        if (!params.is_object())
        {
            err = {"InvalidParams", "Params must be a JSON object."};
            return false;
        }

        if (!apply_bool_fields(params, err, {
                {"verbose", &config.global.verbose},
                {"sarif_format", &config.global.hasSarifFormat},
                {"static_analysis", &config.global.hasStaticAnalysis},
                {"dynamic_analysis", &config.global.hasDynamicAnalysis},
            }))
        {
            return false;
        }
        if (!apply_async_field(params, config, err))
        {
            return false;
        }
        if (!apply_string_fields(params, err, {
                {"report_file", &config.global.report_file},
                {"output_file", &config.global.output_file},
                {"ipc_path", &config.global.ipcPath},
            }))
        {
            return false;
        }
        if (!apply_list_param(params, "entry_points", err, [&](const std::vector<std::string>& values) {
                config.global.entry_points = join_with_comma(values);
            }))
        {
            return false;
        }
        if (!apply_list_param(params, "invoke", err, [&](const std::vector<std::string>& values) {
                config.global.hasInvokedSpecificTools = true;
                config.global.specificTools = values;
            }))
        {
            return false;
        }
        if (!apply_list_param(params, "input", err, [&](const std::vector<std::string>& values) {
                for (const auto& file : values)
                {
                    if (!file.empty())
                    {
                        config.addFile(file);
                    }
                }
            }))
        {
            return false;
        }
        if (!apply_ipc_field(params, config, err))
        {
            return false;
        }

        return true;
    }

    static bool run_analysis(const ctrace::ProgramConfig& config, ILogger& logger, json& result, ParseError& err)
    {
        if (config.files.empty())
        {
            err = {"MissingInput", "Input files are required for analysis."};
            return false;
        }
        if (!config.global.hasStaticAnalysis && !config.global.hasDynamicAnalysis && !config.global.hasInvokedSpecificTools)
        {
            err = {"NoAnalysisSelected", "Enable static_analysis, dynamic_analysis, or invoke tools."};
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
        auto output_capture = std::make_shared<ctrace::Thread::Output::CaptureBuffer>();
        ctrace::ToolInvoker invoker(config, pool_size, config.global.hasAsync, output_capture);

        size_t processed = 0;
        for (const auto& file : config.files)
        {
            if (file.src_file.empty())
            {
                continue;
            }
            ++processed;
            if (config.global.hasStaticAnalysis)
            {
                invoker.runStaticTools(file.src_file);
            }
            if (config.global.hasDynamicAnalysis)
            {
                invoker.runDynamicTools(file.src_file);
            }
            if (config.global.hasInvokedSpecificTools)
            {
                invoker.runSpecificTools(config.global.specificTools, file.src_file);
            }
        }

        if (processed == 0)
        {
            err = {"MissingInput", "Input files are required for analysis."};
            return false;
        }

        logger.info("Analysis completed for " + std::to_string(processed) + " file(s).");

        result["files"] = processed;
        result["static_analysis"] = config.global.hasStaticAnalysis;
        result["dynamic_analysis"] = config.global.hasDynamicAnalysis;
        result["invoked_tools"] = config.global.specificTools;
        result["sarif_format"] = config.global.hasSarifFormat;
        result["report_file"] = config.global.report_file;
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
            baseResponse["error"] = {
                {"code", err.code},
                {"message", err.message}
            };
            return baseResponse;
        }

        json result;
        if (!run_analysis(config, logger_, result, err))
        {
            baseResponse["status"] = "error";
            baseResponse["error"] = {
                {"code", err.code},
                {"message", err.message}
            };
            return baseResponse;
        }

        baseResponse["status"] = "ok";
        baseResponse["result"] = result;
        return baseResponse;
    }
};

// ============================================================================
// Couche HTTP / Transport
// ============================================================================

class HttpServer
{
    public:
        HttpServer(ApiHandler& apiHandler, ILogger& logger)
            : apiHandler_(apiHandler), logger_(logger)
        {}

        void run(const std::string& host, int port)
        {
            // CORS
            server_.Options("/api", [this](const httplib::Request&, httplib::Response& res) {
                set_cors(res);
                res.status = 200;
            });

            // Endpoint principal
            server_.Post("/api", [this](const httplib::Request& req, httplib::Response& res) {
                set_cors(res);
                handle_post_api(req, res);
            });

            logger_.info("[SERVER] Listening on http://" + host + ":" + std::to_string(port));
            server_.listen(host.c_str(), port);
        }

    private:
        httplib::Server server_;
        ApiHandler&     apiHandler_;
        ILogger&        logger_;

        static void set_cors(httplib::Response& res)
        {
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
        }

        void handle_post_api(const httplib::Request& req, httplib::Response& res)
        {
            try {
                json request = json::parse(req.body);
                json response = apiHandler_.handle_request(request);

                res.status = 200;
                res.set_content(response.dump(), "application/json");
            } catch (const std::exception& e) {
                logger_.error(std::string("Exception while handling /api: ") + e.what());

                json err;
                err["proto"]  = "coretrace-1.0";
                err["type"]   = "response";
                err["status"] = "error";
                err["error"]  = {
                    {"code", "InvalidRequest"},
                    {"message", e.what()}
                };

                res.status = 400;
                res.set_content(err.dump(), "application/json");
            }
        }
};
