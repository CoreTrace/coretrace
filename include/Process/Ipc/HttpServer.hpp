// SPDX-License-Identifier: Apache-2.0
#ifndef PROCESS_IPC_HTTP_SERVER_HPP
#define PROCESS_IPC_HTTP_SERVER_HPP

#include "Config/config.hpp"
#include "Process/Ipc/ApiHandler.hpp"
#include "Process/Ipc/GracefulShutdown.hpp"

#include "httplib.h"

#include <string>

/// HTTP transport for the coretrace-1.0 protocol: routes, cross-origin policy, body limit and
/// the token check on the shutdown endpoint. Request handling is ApiHandler's, the drain is
/// GracefulShutdown's.
class HttpServer
{
  public:
    HttpServer(ApiHandler& apiHandler, ILogger& logger, const ctrace::ServerConfig& config);

    /// Stops accepting connections. Safe to call from another thread.
    void stop();

    /// Serves until stopped, then completes the graceful shutdown.
    void run(const std::string& host, int port);

  private:
    void set_cors(httplib::Response& res) const;
    [[nodiscard]] bool is_authorized_shutdown(const httplib::Request& req) const;
    void handle_options(httplib::Response& res) const;
    void handle_post_api(const httplib::Request& req, httplib::Response& res);
    void handle_post_shutdown(const httplib::Request& req, httplib::Response& res);

    httplib::Server server_;
    ApiHandler& apiHandler_;
    ILogger& logger_;
    GracefulShutdown shutdown_;
    std::string shutdown_token_;
    std::string cors_origin_;
};

#endif // PROCESS_IPC_HTTP_SERVER_HPP
