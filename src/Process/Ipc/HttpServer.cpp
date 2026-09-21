// SPDX-License-Identifier: Apache-2.0
#include "Process/Ipc/HttpServer.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <exception>
#include <iostream>
#include <string>

using json = nlohmann::json;

namespace
{
    json errorEnvelope(const char* code, const std::string& message)
    {
        json err;
        err["proto"] = "coretrace-1.0";
        err["type"] = "response";
        err["status"] = "error";
        err["error"] = {{"code", code}, {"message", message}};
        return err;
    }
} // namespace

HttpServer::HttpServer(ApiHandler& apiHandler, ILogger& logger, const ctrace::ServerConfig& config)
    : apiHandler_(apiHandler), logger_(logger),
      shutdown_(logger, std::chrono::milliseconds(config.shutdown_timeout_ms)),
      shutdown_token_(config.shutdown_token), cors_origin_(config.cors_origin)
{
    // Requests carry a file list, not file contents; cpp-httplib is otherwise unbounded.
    if (config.max_body_bytes > 0)
    {
        server_.set_payload_max_length(static_cast<size_t>(config.max_body_bytes));
    }
}

void HttpServer::stop()
{
    server_.stop();
}

void HttpServer::run(const std::string& host, int port)
{
    server_.Options("/api", [this](const httplib::Request&, httplib::Response& res)
                    { handle_options(res); });
    server_.Post("/api", [this](const httplib::Request& req, httplib::Response& res)
                 { handle_post_api(req, res); });
    server_.Options("/shutdown", [this](const httplib::Request&, httplib::Response& res)
                    { handle_options(res); });
    server_.Post("/shutdown", [this](const httplib::Request& req, httplib::Response& res)
                 { handle_post_shutdown(req, res); });

    logger_.info("Listening on http://" + host + ":" + std::to_string(port));
    server_.listen(host.c_str(), port);
    shutdown_.finish();
    std::cout << std::flush;
    std::cerr << std::flush;
}

/// Cross-origin access is opt-in: without a configured origin no header is sent, so a page on
/// another origin cannot read the response.
void HttpServer::set_cors(httplib::Response& res) const
{
    if (cors_origin_.empty())
    {
        return;
    }
    res.set_header("Access-Control-Allow-Origin", cors_origin_);
    res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

bool HttpServer::is_authorized_shutdown(const httplib::Request& req) const
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

void HttpServer::handle_options(httplib::Response& res) const
{
    set_cors(res);
    res.status = shutdown_.is_shutting_down() ? 503 : 200;
}

void HttpServer::handle_post_api(const httplib::Request& req, httplib::Response& res)
{
    set_cors(res);
    if (shutdown_.is_shutting_down())
    {
        res.status = 503;
        res.set_content(errorEnvelope("ServerShuttingDown", "Server is shutting down.").dump(),
                        "application/json");
        return;
    }

    const GracefulShutdown::RequestGuard guard(shutdown_);
    try
    {
        const json request = json::parse(req.body);
        const json response = apiHandler_.handle_request(request);
        res.status = 200;
        res.set_content(response.dump(), "application/json");
    }
    catch (const std::exception& e)
    {
        logger_.error(std::string("Exception while handling /api: ") + e.what());
        res.status = 400;
        res.set_content(errorEnvelope("InvalidRequest", e.what()).dump(), "application/json");
    }
}

void HttpServer::handle_post_shutdown(const httplib::Request& req, httplib::Response& res)
{
    set_cors(res);
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

    json ok;
    ok["status"] = "accepted";
    ok["timeout_ms"] = shutdown_.timeout().count();
    res.status = 202;

    if (!shutdown_.request([this] { server_.stop(); }))
    {
        ok["message"] = "Shutdown already in progress.";
        res.set_content(ok.dump(), "application/json");
        return;
    }
    ok["message"] = "Shutdown initiated.";
    res.set_content(ok.dump(), "application/json");
}
