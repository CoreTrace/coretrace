// SPDX-License-Identifier: Apache-2.0
#include "Process/Ipc/HttpServer.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
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
        err["proto"] = ApiHandler::kProtocol;
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

    /// The one address `host` is bound to: the first that getaddrinfo() gives, in numeric form;
    /// nothing when the host does not resolve. Given a name, cpp-httplib would try the next
    /// address when the first is taken, so that a second server on "localhost" started on the
    /// IPv4 or IPv6 address the first one left free. The first address depends on the system's
    /// resolver configuration: binding only it removes that fallback, it does not promise the
    /// same choice to two resolutions made under different configurations.
    std::optional<std::string> bindAddress(const std::string& host)
    {
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* found = nullptr;
        // An empty host is the loopback address, as for cpp-httplib (no AI_PASSIVE).
        if (::getaddrinfo(host.empty() ? nullptr : host.c_str(), "0", &hints, &found) != 0 ||
            found == nullptr)
        {
            return std::nullopt;
        }
        const std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> addresses(found,
                                                                             &::freeaddrinfo);
        char numeric[NI_MAXHOST];
        if (::getnameinfo(found->ai_addr, found->ai_addrlen, numeric, sizeof(numeric), nullptr, 0,
                          NI_NUMERICHOST) != 0)
        {
            return std::nullopt;
        }
        return std::string(numeric);
    }

    /// `address` as the host of a URL: an IPv6 address goes between brackets.
    std::string urlHost(const std::string& address)
    {
        return address.find(':') == std::string::npos ? address : "[" + address + "]";
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

    const std::string requested = host + ":" + std::to_string(port);
    const std::optional<std::string> address = bindAddress(host);
    if (!address)
    {
        logger_.error("Cannot listen on " + requested + ": the host does not resolve.");
        return false;
    }
    // Port 0 lets the system choose one, which bind_to_any_port() returns.
    const int bound = port == 0 ? server_.bind_to_any_port(*address)
                                : (server_.bind_to_port(*address, port) ? port : -1);
    if (bound <= 0)
    {
        logger_.error("Cannot listen on " + requested +
                      ": the port is already in use or the address is not available.");
        return false;
    }
    logger_.info("Listening on http://" + urlHost(*address) + ":" + std::to_string(bound));
    // False when accepting failed, rather than the listener being stopped.
    const bool served = server_.listen_after_bind();
    if (!served)
    {
        logger_.error("Stopped accepting connections on " + requested + " after an error.");
    }
    shutdown_.finish();
    std::cout << std::flush;
    std::cerr << std::flush;
    return served;
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
