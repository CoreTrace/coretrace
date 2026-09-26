// SPDX-License-Identifier: Apache-2.0
//
// The server as the command line starts it (`ctrace --ipc serve`): it answers an analysis over
// HTTP, refuses a shutdown without its token, and exits with code 0 once asked with it.
//
// Usage: ctrace_server_cli_tests <ctrace> <repository root>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace
{
    using json = nlohmann::json;

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

    constexpr const char* kShutdownToken = "server-cli-test-token";

    /// A loopback port nobody listens on, as the kernel hands them out.
    int freePort()
    {
        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        socklen_t length = sizeof(addr);
        int port = -1;
        if (fd != -1 && ::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0 &&
            ::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &length) == 0)
        {
            port = ntohs(addr.sin_port);
        }
        ::close(fd);
        return port;
    }

    /// `ctrace --ipc serve` for the duration of the scope; killed if it is still running then,
    /// so that a failing test never leaves a server behind.
    class ServerProcess
    {
      public:
        ServerProcess(const std::string& ctrace, int port)
        {
            std::vector<std::string> args = {ctrace,
                                             "--ipc",
                                             "serve",
                                             "--serve-host",
                                             "127.0.0.1",
                                             "--serve-port",
                                             std::to_string(port),
                                             "--shutdown-token",
                                             kShutdownToken};
            std::vector<char*> argv;
            for (std::string& arg : args)
            {
                argv.push_back(arg.data());
            }
            argv.push_back(nullptr);
            if (::posix_spawn(&pid_, ctrace.c_str(), nullptr, nullptr, argv.data(), environ) != 0)
            {
                pid_ = -1;
            }
        }

        ~ServerProcess()
        {
            if (pid_ > 0 && !exited_)
            {
                ::kill(pid_, SIGKILL);
                ::waitpid(pid_, nullptr, 0);
            }
        }

        ServerProcess(const ServerProcess&) = delete;
        ServerProcess& operator=(const ServerProcess&) = delete;

        [[nodiscard]] bool started() const
        {
            return pid_ > 0;
        }

        /// The exit code once the process ended within `timeout` (128 + the signal when a
        /// signal ended it); nothing while it still runs.
        [[nodiscard]] std::optional<int> waitExit(std::chrono::milliseconds timeout)
        {
            const auto deadline = std::chrono::steady_clock::now() + timeout;
            do
            {
                int status = 0;
                if (pid_ > 0 && ::waitpid(pid_, &status, WNOHANG) == pid_)
                {
                    exited_ = true;
                    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            } while (std::chrono::steady_clock::now() < deadline);
            return std::nullopt;
        }

      private:
        pid_t pid_ = -1;
        bool exited_ = false;
    };

    bool waitUntilServing(httplib::Client& client)
    {
        for (int attempt = 0; attempt < 200; ++attempt)
        {
            if (const auto probe = client.Options("/api"); probe && probe->status == 200)
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return false;
    }

    json analysisRequest(const std::filesystem::path& repoRoot,
                         const std::filesystem::path& reportFile)
    {
        return {{"proto", "coretrace-1.0"},
                {"id", 1},
                {"type", "request"},
                {"method", "run_analysis"},
                {"params",
                 {{"input", json::array({(repoRoot / "tests/double_free.c").string()})},
                  {"config", (repoRoot / "config/tool-config.json").string()},
                  {"invoke", json::array({"ctrace_stack_analyzer"})},
                  {"report_file", reportFile.string()}}}};
    }

    void checkAnalysis(TestReport& report, httplib::Client& client,
                       const std::filesystem::path& repoRoot)
    {
        const std::filesystem::path reportFile =
            std::filesystem::temp_directory_path() /
            ("ctrace-server-cli-" + std::to_string(::getpid()) + ".json");
        const auto answer =
            client.Post("/api", analysisRequest(repoRoot, reportFile).dump(), "application/json");
        std::filesystem::remove(reportFile);

        const json response =
            answer ? json::parse(answer->body, nullptr, /*allow_exceptions=*/false) : json();
        report.expect(answer && answer->status == 200 && response.is_object() &&
                          response.value("status", "") == "ok",
                      "an analysis request is answered with status ok");
        const json diagnostics =
            response.is_object()
                ? response.value("result", json::object()).value("diagnostics", json::array())
                : json::array();
        report.expect(
            std::any_of(
                diagnostics.begin(), diagnostics.end(), [](const json& diagnostic)
                { return diagnostic.value("rule_id", "") == "ResourceLifetime.DoubleRelease"; }),
            "the answer carries the stack analyzer's double release finding");
    }

    void checkShutdown(TestReport& report, httplib::Client& client, ServerProcess& server)
    {
        const auto anonymous = client.Post("/shutdown", "", "application/json");
        report.expect(anonymous && anonymous->status == 403,
                      "a shutdown without the token is refused");
        report.expect(!server.waitExit(std::chrono::milliseconds(500)).has_value(),
                      "the server keeps running after a refused shutdown");

        const httplib::Headers authorization = {
            {"Authorization", std::string("Bearer ") + kShutdownToken}};
        const auto authorized = client.Post("/shutdown", authorization, "", "application/json");
        report.expect(authorized && authorized->status == 202,
                      "a shutdown with the token is accepted");
    }
} // namespace

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: ctrace_server_cli_tests <ctrace> <repository root>\n";
        return 2;
    }
    const std::string ctrace = argv[1];
    const std::filesystem::path repoRoot = argv[2];
    TestReport report;

    const int port = freePort();
    ServerProcess server(ctrace, port);
    report.expect(port > 0 && server.started(), "ctrace --ipc serve starts");

    httplib::Client client("127.0.0.1", port);
    client.set_read_timeout(60, 0);
    const bool serving = waitUntilServing(client);
    report.expect(serving, "the server answers on the port it was given");
    if (serving)
    {
        checkAnalysis(report, client, repoRoot);
        checkShutdown(report, client, server);
    }

    const std::optional<int> exitCode = server.waitExit(std::chrono::seconds(30));
    report.expect(exitCode == 0, "the server exits with code 0 once shut down (exit " +
                                     (exitCode ? std::to_string(*exitCode) : "none") + ")");

    if (report.failures == 0)
    {
        std::cout << "server_cli_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " server CLI check(s) failed\n";
    return 1;
}
