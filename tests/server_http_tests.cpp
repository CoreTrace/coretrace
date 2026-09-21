// SPDX-License-Identifier: Apache-2.0
//
// Transport-level policy of the HTTP server: what it refuses to start, how large a request it
// accepts, and which cross-origin headers it emits.
#include "App/Runner.hpp"
#include "Process/Ipc/HttpServer.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace
{
    struct TestReport
    {
        int failures = 0;

        void expect(bool condition, const std::string& message)
        {
            if (condition)
            {
                std::cout << "[PASS] " << message << "\n";
                return;
            }
            ++failures;
            std::cerr << "[FAIL] " << message << "\n";
        }
    };

    /// Runs a server on a port for the duration of the scope.
    class ServerFixture
    {
      public:
        explicit ServerFixture(const ctrace::ServerConfig& config)
            : handler_(logger_), server_(handler_, logger_, config), config_(config)
        {
            thread_ = std::thread(
                [this]
                {
                    server_.run(config_.host, config_.port);
                    runReturned_.store(true);
                });
            httplib::Client probe(config_.host, config_.port);
            probe.set_connection_timeout(0, 100000);
            for (int attempt = 0; attempt < 100; ++attempt)
            {
                if (probe.Options("/api"))
                {
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }

        ~ServerFixture()
        {
            server_.stop();
            if (thread_.joinable())
            {
                thread_.join();
            }
        }

        /// True once run() has returned, i.e. the listener stopped and the drain completed.
        [[nodiscard]] bool finished(std::chrono::milliseconds within)
        {
            const auto deadline = std::chrono::steady_clock::now() + within;
            while (std::chrono::steady_clock::now() < deadline)
            {
                if (runReturned_.load())
                {
                    return true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return runReturned_.load();
        }

        [[nodiscard]] httplib::Client client() const
        {
            httplib::Client client(config_.host, config_.port);
            client.set_read_timeout(30, 0);
            return client;
        }

      private:
        ConsoleLogger logger_;
        ApiHandler handler_;
        HttpServer server_;
        ctrace::ServerConfig config_;
        std::thread thread_;
        std::atomic<bool> runReturned_{false};
    };

    ctrace::ServerConfig localConfig(int port)
    {
        ctrace::ServerConfig config;
        config.host = "127.0.0.1";
        config.port = port;
        return config;
    }

    // A server reachable from the network with no shutdown token must not start.
    void testBindPolicy(TestReport& report)
    {
        ctrace::ServerConfig exposed = localConfig(8080);
        exposed.host = "0.0.0.0";
        const std::string error = ctrace::validateServerConfig(exposed);
        report.expect(!error.empty() && error.find("shutdown_token") != std::string::npos,
                      "a non-loopback bind without a token is refused (" + error + ")");

        exposed.shutdown_token = "secret";
        report.expect(ctrace::validateServerConfig(exposed).empty(),
                      "a non-loopback bind with a token is allowed");

        for (const char* host : {"127.0.0.1", "localhost", "::1", ""})
        {
            ctrace::ServerConfig loopback = localConfig(8080);
            loopback.host = host;
            report.expect(ctrace::validateServerConfig(loopback).empty(),
                          std::string("loopback host '") + host + "' needs no token");
        }
    }

    void testBodyLimit(TestReport& report)
    {
        ctrace::ServerConfig config = localConfig(18801);
        config.max_body_bytes = 1024;
        const ServerFixture server(config);
        auto client = server.client();

        const std::string small =
            R"({"proto":"coretrace-1.0","id":1,"type":"request","method":"x"})";
        const auto accepted = client.Post("/api", small, "application/json");
        report.expect(accepted && accepted->status == 200, "a request under the limit is accepted");

        const std::string large = R"({"pad":")" + std::string(4096, 'x') + R"("})";
        const auto rejected = client.Post("/api", large, "application/json");
        report.expect(rejected && rejected->status == 413,
                      "a request over the limit is rejected with 413 (got " +
                          (rejected ? std::to_string(rejected->status) : "no response") + ")");
    }

    void testCorsHeaders(TestReport& report)
    {
        {
            const ServerFixture server(localConfig(18802));
            const auto response = server.client().Options("/api");
            report.expect(response && !response->has_header("Access-Control-Allow-Origin"),
                          "no cross-origin header by default");
        }
        {
            ctrace::ServerConfig config = localConfig(18803);
            config.cors_origin = "https://gui.example";
            const ServerFixture server(config);
            const auto response = server.client().Options("/api");
            report.expect(response && response->get_header_value("Access-Control-Allow-Origin") ==
                                          "https://gui.example",
                          "the configured origin is echoed back");
        }
    }
    // K5: the shutdown endpoint is token-protected and stops the server gracefully.
    void testGracefulShutdown(TestReport& report)
    {
        {
            const ServerFixture server(localConfig(18804));
            const auto response = server.client().Post("/shutdown", "", "application/json");
            report.expect(response && response->status == 403 &&
                              response->body.find("Shutdown token not configured.") !=
                                  std::string::npos,
                          "shutdown without a configured token is refused with 403");
        }

        ctrace::ServerConfig config = localConfig(18805);
        config.shutdown_token = "s3cret";
        ServerFixture server(config);
        auto client = server.client();

        const auto wrong =
            client.Post("/shutdown", {{"Authorization", "Bearer nope"}}, "", "application/json");
        report.expect(wrong && wrong->status == 403 &&
                          wrong->body.find("Invalid shutdown token.") != std::string::npos,
                      "shutdown with a wrong token is refused with 403");
        report.expect(!server.finished(std::chrono::milliseconds(200)),
                      "a refused shutdown leaves the server running");

        const auto accepted =
            client.Post("/shutdown", {{"X-Admin-Token", "s3cret"}}, "", "application/json");
        report.expect(accepted && accepted->status == 202 &&
                          accepted->body.find("Shutdown initiated.") != std::string::npos,
                      "shutdown with the token is accepted with 202");
        report.expect(server.finished(std::chrono::seconds(5)),
                      "the server stops after an accepted shutdown");
    }
} // namespace

int main()
{
    coretrace::enable_logging();
    coretrace::set_min_level(coretrace::Level::Error);

    TestReport report;
    testBindPolicy(report);
    testBodyLimit(report);
    testCorsHeaders(report);
    testGracefulShutdown(report);

    if (report.failures == 0)
    {
        std::cout << "server_http_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " server http check(s) failed\n";
    return 1;
}
