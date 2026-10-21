// SPDX-License-Identifier: Apache-2.0
//
// Transport-level policy of the HTTP server: what it refuses to start, how large a request it
// accepts, and which cross-origin headers it emits.
#include "App/Runner.hpp"
#include "Process/Ipc/HttpServer.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <utility>

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
            : handler_(logger_, /*executable=*/{}), server_(handler_, logger_, config),
              config_(config)
        {
            thread_ = std::thread(
                [this]
                {
                    static_cast<void>(server_.run(config_.host, config_.port));
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
        report.expect(accepted && accepted->status == 400 &&
                          accepted->body.find("UnknownMethod") != std::string::npos,
                      "a request under the limit reaches the handler");

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
    /// Whether a second server starts on `config`, where a first one listens. run() returns
    /// false at once when the bind fails; a server that binds serves until it is stopped, so
    /// one still running after a moment is stopped, and counted as started.
    [[nodiscard]] bool secondServerStarts(const ctrace::ServerConfig& config)
    {
        ConsoleLogger logger;
        ApiHandler handler(logger, /*executable=*/{});
        HttpServer second(handler, logger, config);
        std::promise<bool> ran;
        std::future<bool> result = ran.get_future();
        std::thread thread([&] { ran.set_value(second.run(config.host, config.port)); });
        const bool returned = result.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
        if (!returned)
        {
            second.stop();
        }
        thread.join();
        return !returned || result.get();
    }

    // S19: a port another server listens on is an error, not a second listener. "localhost" has
    // an IPv4 and an IPv6 address: the second server must not fall back to the one left free.
    void testPortInUse(TestReport& report)
    {
        for (const auto& [host, port] :
             {std::pair{"127.0.0.1", 18806}, std::pair{"localhost", 18809}})
        {
            ctrace::ServerConfig config = localConfig(port);
            config.host = host;
            const ServerFixture first(config);
            report.expect(!secondServerStarts(config), std::string("a second server on ") + host +
                                                           ", port in use, does not start");
        }
    }

    // S04, S05: errors are told by the HTTP status too, and the error object stays in the body.
    void testErrorStatuses(TestReport& report)
    {
        const ServerFixture server(localConfig(18807));
        auto client = server.client();
        const auto post = [&client](const nlohmann::json& params, const std::string& proto,
                                    const std::string& method)
        {
            return client.Post("/api",
                               nlohmann::json{{"proto", proto},
                                              {"id", 7},
                                              {"type", "request"},
                                              {"method", method},
                                              {"params", params}}
                                   .dump(),
                               "application/json");
        };
        const auto expectAnswer = [&report](const httplib::Result& answer, int status,
                                            const std::string& code, const std::string& what)
        {
            const nlohmann::json body =
                answer ? nlohmann::json::parse(answer->body, nullptr, false) : nlohmann::json();
            const std::string got = answer ? std::to_string(answer->status) : "no response";
            report.expect(answer && answer->status == status && body.is_object() &&
                              body.value("error", nlohmann::json::object()).value("code", "") ==
                                  code,
                          what + " is answered with " + std::to_string(status) + " " + code +
                              " (got " + got + ")");
        };

        const nlohmann::json noInput = {{"static_analysis", true}};
        expectAnswer(post(noInput, "coretrace-9.9", "run_analysis"), 400, "UnsupportedProtocol",
                     "an unsupported proto");
        expectAnswer(post(noInput, "coretrace-1.0", "no_such_method"), 400, "UnknownMethod",
                     "an unknown method");
        expectAnswer(post({{"static_analysis", true}, {"no_such_param", 1}}, "coretrace-1.0",
                          "run_analysis"),
                     400, "InvalidParams", "an unknown parameter");
        expectAnswer(post({{"jobs", "many"}}, "coretrace-1.0", "run_analysis"), 400,
                     "InvalidParams", "a parameter of the wrong type");
        expectAnswer(post(nlohmann::json::object(), "coretrace-1.0", "run_analysis"), 400,
                     "NoAnalysisSelected", "a request without analysis");
        expectAnswer(post(noInput, "coretrace-1.0", "run_analysis"), 400, "MissingInput",
                     "a request without input");
        expectAnswer(
            post({{"dynamic_analysis", true}, {"input", "a.c"}}, "coretrace-1.0", "run_analysis"),
            403, "DynamicAnalysisDisabled", "dynamic analysis without the opt-in");
        expectAnswer(client.Post("/api", "{not json", "application/json"), 400, "InvalidRequest",
                     "malformed JSON");
        // A Latin-1 file name: the parser's message quotes the invalid byte.
        expectAnswer(
            client.Post("/api",
                        "{\"proto\":\"coretrace-1.0\",\"params\":{\"input\":\"caf\xe9.c\"}}",
                        "application/json"),
            400, "InvalidRequest", "a body that is not valid UTF-8");
        expectAnswer(client.Post("/api", R"({"id":1e1000})", "application/json"), 400,
                     "InvalidRequest", "a number out of range");
        expectAnswer(client.Post("/api", "[1]", "application/json"), 400, "InvalidRequest",
                     "a request that is not an object");
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

    /// A run_analysis request for `file` with the tools `invoke`, `dynamic` or not, and a config
    /// file of the request's own.
    nlohmann::json analysisRequest(const std::string& file, const nlohmann::json& invoke,
                                   bool dynamic, const std::string& configFile)
    {
        nlohmann::json params = {{"input", nlohmann::json::array({file})},
                                 {"invoke", invoke},
                                 {"dynamic_analysis", dynamic},
                                 {"config", configFile}};
        return {{"proto", "coretrace-1.0"},
                {"id", 1},
                {"type", "request"},
                {"method", "run_analysis"},
                {"params", params}};
    }

    [[nodiscard]] std::string errorCode(const nlohmann::json& response)
    {
        return response.value("error", nlohmann::json::object()).value("code", "");
    }

    // Dynamic analysis runs the client's code: a server refuses it unless it was started with
    // the opt-in, and a request cannot grant itself the opt-in through its own config file.
    void testDynamicAnalysisNeedsTheServerOptIn(TestReport& report, const std::string& buildDir,
                                                const std::string& sourceDir)
    {
        const std::string source = sourceDir + "/tests/runtime/clean.c";
        const std::string fakeTool = buildDir + "/fake-runtime-analyzer.json";
        const std::string grantsItself = buildDir + "/server-grants-dynamic.json";
        {
            std::ofstream(grantsItself)
                << R"({"schema_version": 1, "server": {"allow_dynamic_analysis": true}})";
        }
        ConsoleLogger logger;

        ApiHandler closed(logger, /*executable=*/{});
        report.expect(errorCode(closed.handle_request(
                          analysisRequest(source, nlohmann::json::array(), true, fakeTool))) ==
                          "DynamicAnalysisDisabled",
                      "dynamic_analysis is refused without the server's opt-in");
        report.expect(errorCode(closed.handle_request(analysisRequest(
                          source, {"coretrace-runtime-analyzer"}, false, fakeTool))) ==
                          "DynamicAnalysisDisabled",
                      "invoking the runtime analyzer is refused without the opt-in");
        report.expect(errorCode(closed.handle_request(
                          analysisRequest(source, nlohmann::json::array(), true, grantsItself))) ==
                          "DynamicAnalysisDisabled",
                      "a request's own config cannot grant the opt-in");

        ApiHandler open(logger, /*executable=*/{}, /*allowDynamicAnalysis=*/true);
        const nlohmann::json response =
            open.handle_request(analysisRequest(source, nlohmann::json::array(), true, fakeTool));
        report.expect(response.value("status", "") == "ok",
                      "with the opt-in, the dynamic analysis runs (" +
                          response.value("error", nlohmann::json::object()).dump() + ")");
    }

    // The IPC socket is a resource of the server's side: one nobody listens on is a server
    // failure, answered with 500 and no details, not an invalid request.
    void testUnreachableIpcSocket(TestReport& report, const std::string& sourceDir)
    {
        const ServerFixture server(localConfig(18810));
        nlohmann::json request =
            analysisRequest(sourceDir + "/tests/double_free.c", {"cppcheck"}, false, "");
        request["params"].erase("config");
        request["params"]["ipc"] = "socket";
        request["params"]["ipc_path"] = "/nonexistent/ctrace-ipc.sock";
        const auto answer = server.client().Post("/api", request.dump(), "application/json");
        const nlohmann::json body =
            answer ? nlohmann::json::parse(answer->body, nullptr, false) : nlohmann::json();
        report.expect(answer && answer->status == 500 && body.is_object() &&
                          errorCode(body) == "InternalError" &&
                          answer->body.find("ctrace-ipc.sock") == std::string::npos,
                      "an IPC socket nobody listens on is answered with 500 InternalError, "
                      "without its details (got " +
                          (answer
                               ? std::to_string(answer->status) + " " + answer->body.substr(0, 160)
                               : "no response") +
                          ")");
    }

    // A tool may print bytes that are not UTF-8, such as a message in Latin-1: the result still
    // reaches the client, those bytes replaced by U+FFFD, in JSON and in SARIF.
    void testToolOutputThatIsNotUtf8(TestReport& report, const std::string& buildDir,
                                     const std::string& sourceDir)
    {
        const std::string latin1Tool = buildDir + "/fake-cppcheck-latin1.json";
        {
            std::ofstream(latin1Tool)
                << nlohmann::json{{"schema_version", 1},
                                  {"tools",
                                   {{"cppcheck",
                                     {{"path", sourceDir + "/tests/fake-file-list-tool.sh"},
                                      {"args", {"--fake-latin1"}}}}}}}
                       .dump();
        }
        const ServerFixture server(localConfig(18808));
        auto client = server.client();
        const std::string replaced = "caf\xEF\xBF\xBD";
        for (const bool sarif : {false, true})
        {
            nlohmann::json request = analysisRequest(sourceDir + "/tests/double_free.c",
                                                     {"cppcheck"}, false, latin1Tool);
            const std::string reportFile = buildDir + "/latin1-report.sarif";
            std::filesystem::remove(reportFile);
            request["params"]["sarif_format"] = sarif;
            request["params"]["report_file"] = reportFile;
            const auto answer = client.Post("/api", request.dump(), "application/json");
            const std::string format = sarif ? "SARIF" : "JSON";
            report.expect(
                answer && answer->status == 200 && answer->body.find(replaced) != std::string::npos,
                "a tool output that is not UTF-8 keeps the " + format +
                    " result, the byte replaced (got " +
                    (answer ? std::to_string(answer->status) + " " + answer->body.substr(0, 160)
                            : "no response") +
                    ")");
            if (sarif)
            {
                std::ifstream written(reportFile);
                const std::string document((std::istreambuf_iterator<char>(written)),
                                           std::istreambuf_iterator<char>());
                report.expect(document.find(replaced) != std::string::npos,
                              "the SARIF report file is written, the byte replaced");
            }
        }
    }

    // A tool whose output cannot be read leaves the run incomplete, as on the command line:
    // the gate exits 3 and names the tool. echo stands in for a flawfinder that prints no SARIF.
    void testUnreadToolOutput(TestReport& report, const std::string& buildDir,
                              const std::string& sourceDir)
    {
        ConsoleLogger logger;
        ApiHandler handler(logger, /*executable=*/{});
        const nlohmann::json response = handler.handle_request(
            analysisRequest(sourceDir + "/tests/double_free.c", {"flawfinder"}, false,
                            buildDir + "/fake-flawfinder-text.json"));
        const nlohmann::json gate =
            response.value(nlohmann::json::json_pointer("/result/gate"), nlohmann::json::object());
        report.expect(response.value("status", "") == "ok" && gate.value("exit_code", -1) == 3 &&
                          gate.value("unread_tools", nlohmann::json::array()) ==
                              nlohmann::json::array({"flawfinder"}),
                      "a tool whose output cannot be read makes the gate exit 3 and is named (" +
                          gate.dump() + ")");
    }
} // namespace

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: ctrace_server_http_tests <build dir> <source dir>\n";
        return 2;
    }
    coretrace::enable_logging();
    coretrace::set_min_level(coretrace::Level::Error);

    TestReport report;
    testBindPolicy(report);
    testBodyLimit(report);
    testCorsHeaders(report);
    testPortInUse(report);
    testErrorStatuses(report);
    testGracefulShutdown(report);
    testDynamicAnalysisNeedsTheServerOptIn(report, argv[1], argv[2]);
    testToolOutputThatIsNotUtf8(report, argv[1], argv[2]);
    testUnreachableIpcSocket(report, argv[2]);
    testUnreadToolOutput(report, argv[1], argv[2]);

    if (report.failures == 0)
    {
        std::cout << "server_http_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " server http check(s) failed\n";
    return 1;
}
