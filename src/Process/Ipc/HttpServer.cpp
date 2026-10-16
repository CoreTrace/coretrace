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
    /// The text of `value`. A message may quote bytes that are not UTF-8, the parser's or a
    /// tool's: they are replaced, as JSON must be UTF-8, rather than failing the response.
    std::string toJsonText(const json& value)
    {
        return value.dump(-1, ' ', false, json::error_handler_t::replace);
    }

    json errorEnvelope(const char* code, const std::string& message)
    {
        json err;
        err["proto"] = "coretrace-1.0";
        err["type"] = "response";
        err["status"] = "error";
        err["error"] = {{"code", code}, {"message", message}};
        return err;
    }

    /// 200 for a success; for an error, 403 when the server refuses it, 400 otherwise: every
    /// other error code is about the request itself.
    int httpStatusFor(const json& response)
    {
        if (response.value("status", "") == "ok")
        {
            return 200;
        }
        return response.value("error", json::object()).value("code", "") ==
                       "DynamicAnalysisDisabled"
                   ? 403
                   : 400;
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
    // httplib's default adds SO_REUSEPORT, which lets a second server share the port.
    server_.set_socket_options(
        [](socket_t sock)
        {
            const int yes = 1;
            setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes),
                       sizeof(yes));
        });
}

void HttpServer::stop()
{
    server_.stop();
}

bool HttpServer::run(const std::string& host, int port)
{
    server_.Options("/api", [this](const httplib::Request&, httplib::Response& res)
                    { handle_options(res); });
    server_.Post("/api", [this](const httplib::Request& req, httplib::Response& res)
                 { handle_post_api(req, res); });
    server_.Options("/shutdown", [this](const httplib::Request&, httplib::Response& res)
                    { handle_options(res); });
    server_.Post("/shutdown", [this](const httplib::Request& req, httplib::Response& res)
                 { handle_post_shutdown(req, res); });

    const std::string address = host + ":" + std::to_string(port);
    if (!server_.bind_to_port(host, port))
    {
        logger_.error("Cannot listen on " + address +
                      ": the port is already in use or the address is not available.");
        return false;
    }
    logger_.info("Listening on http://" + address);
    server_.listen_after_bind();
    shutdown_.finish();
    std::cout << std::flush;
    std::cerr << std::flush;
    return true;
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
        res.set_content(toJsonText(errorEnvelope("ServerShuttingDown", "Server is shutting down.")),
                        "application/json");
        return;
    }

    const GracefulShutdown::RequestGuard guard(shutdown_);
    try
    {
        json request;
        try
        {
            request = json::parse(req.body);
        }
        // Not only parse_error: a number too large for a double throws out_of_range.
        catch (const json::exception& e)
        {
            res.status = 400;
            res.set_content(toJsonText(errorEnvelope("InvalidRequest", e.what())),
                            "application/json");
            return;
        }
        const json response = apiHandler_.handle_request(request);
        res.status = httpStatusFor(response);
        res.set_content(toJsonText(response), "application/json");
    }
    catch (const std::exception& e)
    {
        // The details stay in the server's log: they describe the server, not the request.
        logger_.error(std::string("Exception while handling /api: ") + e.what());
        res.status = 500;
        res.set_content(toJsonText(errorEnvelope("InternalError", "Internal server error.")),
                        "application/json");
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
