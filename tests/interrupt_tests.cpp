// SPDX-License-Identifier: Apache-2.0
//
// Interrupting ctrace. A run interrupted by SIGINT or SIGTERM stops the tool it runs, with what
// that tool started, removes the tool's run directory, and ends by the signal it received. A
// server interrupted by SIGTERM drains as POST /shutdown does: the analysis in flight is
// answered, then the server ends by the signal.
//
// Usage: ctrace_interrupt_tests <ctrace> <repository root> <work directory>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace
{
    using json = nlohmann::json;
    namespace fs = std::filesystem;

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

    /// A process for the duration of the scope, with SIGINT and SIGTERM at their default action
    /// whatever the test runner left them at; killed if it still runs then.
    class Child
    {
      public:
        /// What the process prints, on stdout and on stderr, goes to `output`.
        Child(std::vector<std::string> args, const fs::path& output)
        {
            std::vector<char*> argv;
            for (std::string& arg : args)
            {
                argv.push_back(arg.data());
            }
            argv.push_back(nullptr);
            posix_spawn_file_actions_t actions;
            ::posix_spawn_file_actions_init(&actions);
            ::posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, output.c_str(),
                                               O_WRONLY | O_CREAT | O_TRUNC, 0644);
            ::posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);
            posix_spawnattr_t attributes;
            ::posix_spawnattr_init(&attributes);
            sigset_t defaults;
            sigemptyset(&defaults);
            sigaddset(&defaults, SIGINT);
            sigaddset(&defaults, SIGTERM);
            ::posix_spawnattr_setsigdefault(&attributes, &defaults);
            ::posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSIGDEF);
            if (::posix_spawn(&pid_, argv[0], &actions, &attributes, argv.data(), environ) != 0)
            {
                pid_ = -1;
            }
            ::posix_spawnattr_destroy(&attributes);
            ::posix_spawn_file_actions_destroy(&actions);
        }

        ~Child()
        {
            if (pid_ > 0 && !ended_)
            {
                ::kill(pid_, SIGKILL);
                ::waitpid(pid_, nullptr, 0);
            }
        }

        Child(const Child&) = delete;
        Child& operator=(const Child&) = delete;

        [[nodiscard]] bool started() const
        {
            return pid_ > 0;
        }

        void signal(int signal) const
        {
            if (pid_ > 0)
            {
                ::kill(pid_, signal);
            }
        }

        /// How the process ended, as waitpid() reports it, once it ended within `timeout`.
        [[nodiscard]] std::optional<int> waitEnd(std::chrono::milliseconds timeout)
        {
            const auto deadline = std::chrono::steady_clock::now() + timeout;
            do
            {
                int status = 0;
                if (pid_ > 0 && ::waitpid(pid_, &status, WNOHANG) == pid_)
                {
                    ended_ = true;
                    return status;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            } while (std::chrono::steady_clock::now() < deadline);
            return std::nullopt;
        }

      private:
        pid_t pid_ = -1;
        bool ended_ = false;
    };

    [[nodiscard]] bool endedBy(const std::optional<int>& status, int signal)
    {
        return status && WIFSIGNALED(*status) && WTERMSIG(*status) == signal;
    }

    [[nodiscard]] std::string signalName(int signal)
    {
        return signal == SIGINT ? "SIGINT" : "SIGTERM";
    }

    /// The files of one test: the fake tool's configuration, the pids it writes, a temporary
    /// directory of its own (TMPDIR) and what ctrace prints.
    struct Workspace
    {
        fs::path tmp;
        fs::path pidFile;
        fs::path config;
        fs::path log;

        Workspace(const fs::path& dir, const fs::path& repoRoot)
            : tmp(dir / "tmp"), pidFile(dir / "pids"), config(dir / "config.json"),
              log(dir / "ctrace.log")
        {
            fs::remove_all(dir);
            fs::create_directories(tmp);
            const json tools = {
                {"schema_version", 1},
                {"tools",
                 {{"cppcheck", {{"path", (repoRoot / "tests/fake-hung-tool.sh").string()}}}}}};
            std::ofstream(config) << tools.dump() << '\n';
            ::setenv("CTRACE_FAKE_PID_FILE", pidFile.c_str(), 1);
        }

        /// The pids of the fake tool and of the sleep it started, once it wrote them.
        [[nodiscard]] std::vector<pid_t> waitForTool() const
        {
            for (int attempt = 0; attempt < 200; ++attempt)
            {
                std::ifstream in(pidFile);
                std::vector<pid_t> pids;
                long pid = 0;
                while (in >> pid)
                {
                    pids.push_back(static_cast<pid_t>(pid));
                }
                if (pids.size() == 2)
                {
                    return pids;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            return {};
        }

        [[nodiscard]] bool hasRunDirectory() const
        {
            for (const fs::directory_entry& entry : fs::directory_iterator(tmp))
            {
                if (entry.path().filename().string().rfind("ctrace-", 0) == 0)
                {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] std::string printed() const
        {
            std::ifstream in(log);
            return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        }
    };

    /// Whether every process of `pids` is gone. One that ended waits a moment for its new
    /// parent to reap it when its own parent ended first, and kill() still finds it until then.
    [[nodiscard]] bool allGone(const std::vector<pid_t>& pids)
    {
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            bool running = false;
            for (const pid_t pid : pids)
            {
                running = running || ::kill(pid, 0) == 0;
            }
            if (!running)
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return false;
    }

    /// What a failing check leaves running would otherwise sleep for ten minutes.
    void killLeftovers(const std::vector<pid_t>& pids)
    {
        for (const pid_t pid : pids)
        {
            ::kill(pid, SIGKILL);
        }
    }

    void testInterruptedRun(TestReport& report, const std::string& ctrace, const fs::path& repoRoot,
                            const fs::path& work, int signal)
    {
        const std::string name = signalName(signal);
        const Workspace workspace(work / ("interrupt-" + name), repoRoot);
        ::setenv("CTRACE_FAKE_SLEEP", "600", 1);
        ::setenv("TMPDIR", workspace.tmp.c_str(), 1);
        Child run({ctrace, "--invoke", "cppcheck", "--config", workspace.config.string(), "--input",
                   (repoRoot / "tests/double_free.c").string()},
                  workspace.log);
        ::unsetenv("TMPDIR");

        const std::vector<pid_t> tool = workspace.waitForTool();
        report.expect(run.started() && tool.size() == 2 && workspace.hasRunDirectory(),
                      name + ": the tool runs, with its run directory");
        run.signal(signal);
        const std::optional<int> status = run.waitEnd(std::chrono::seconds(10));
        report.expect(endedBy(status, signal), name + ": ctrace ends by the signal it received");
        report.expect(allGone(tool), name + ": the tool, and the process it started, are stopped");
        report.expect(!workspace.hasRunDirectory(), name + ": the run directory is removed");
        killLeftovers(tool);
        std::cout << workspace.printed();
    }

    // A tool that ended before ctrace acted on the interrupt may leave what it started behind,
    // in its process group: that group is stopped all the same. The tool interrupts ctrace, then
    // exits. With one job, the thread that waits on the tool handles the signal before it can see
    // the tool end; with several, another thread may handle it after the tool was seen ending.
    void testToolThatEndedFirst(TestReport& report, const std::string& ctrace,
                                const fs::path& repoRoot, const fs::path& work, bool oneJob)
    {
        const std::string name = oneJob ? "ended first, one job" : "ended first, parallel";
        const Workspace workspace(
            work / (oneJob ? "interrupt-ended-first-1" : "interrupt-ended-first"), repoRoot);
        ::setenv("CTRACE_FAKE_SLEEP", "600", 1);
        ::setenv("CTRACE_FAKE_INTERRUPT_PARENT", "1", 1);
        ::setenv("TMPDIR", workspace.tmp.c_str(), 1);
        std::vector<std::string> args = {ctrace,
                                         "--invoke",
                                         "cppcheck",
                                         "--config",
                                         workspace.config.string(),
                                         "--input",
                                         (repoRoot / "tests/double_free.c").string()};
        if (oneJob)
        {
            args.insert(args.begin() + 1, {"--jobs", "1"});
        }
        Child run(args, workspace.log);
        ::unsetenv("TMPDIR");
        ::unsetenv("CTRACE_FAKE_INTERRUPT_PARENT");

        const std::vector<pid_t> tool = workspace.waitForTool();
        const std::optional<int> status = run.waitEnd(std::chrono::seconds(10));
        report.expect(tool.size() == 2 && endedBy(status, SIGTERM),
                      name + ": ctrace ends by the signal");
        report.expect(allGone(tool), name + ": what the tool left in its group is stopped");
        killLeftovers(tool);
        std::cout << workspace.printed();
    }

    // Once interrupted, a run starts no other tool: a linked analyzer, which cannot be stopped,
    // would keep ctrace running for as long as it takes.
    void testNoToolStartsAfterInterrupt(TestReport& report, const std::string& ctrace,
                                        const fs::path& repoRoot, const fs::path& work)
    {
        const Workspace workspace(work / "interrupt-no-start", repoRoot);
        ::setenv("CTRACE_FAKE_SLEEP", "600", 1);
        ::setenv("TMPDIR", workspace.tmp.c_str(), 1);
        Child run({ctrace, "--jobs", "1", "--invoke", "cppcheck,ctrace_stack_analyzer", "--config",
                   workspace.config.string(), "--input",
                   (repoRoot / "tests/double_free.c").string()},
                  workspace.log);
        ::unsetenv("TMPDIR");

        const std::vector<pid_t> tool = workspace.waitForTool();
        run.signal(SIGINT);
        const std::optional<int> status = run.waitEnd(std::chrono::seconds(30));
        const std::string printed = workspace.printed();
        report.expect(tool.size() == 2 && endedBy(status, SIGINT),
                      "no start: ctrace ends by the signal");
        report.expect(printed.find("Running CoreTrace Stack Analyzer") == std::string::npos,
                      "no start: the stack analyzer does not start after the interrupt");
        killLeftovers(tool);
        std::cout << printed;
    }

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

    void testServerDrains(TestReport& report, const std::string& ctrace, const fs::path& repoRoot,
                          const fs::path& work)
    {
        const Workspace workspace(work / "interrupt-server", repoRoot);
        // Long enough to be in flight when SIGTERM arrives, short enough to be waited for.
        ::setenv("CTRACE_FAKE_SLEEP", "2", 1);
        const int port = freePort();
        Child server({ctrace, "--ipc", "serve", "--serve-host", "127.0.0.1", "--serve-port",
                      std::to_string(port)},
                     workspace.log);
        httplib::Client probe("127.0.0.1", port);
        report.expect(port > 0 && server.started() && waitUntilServing(probe),
                      "server: answers on its port");

        const json request = {
            {"proto", "coretrace-1.0"},
            {"id", 1},
            {"type", "request"},
            {"method", "run_analysis"},
            {"params",
             {{"input", json::array({(repoRoot / "tests/double_free.c").string()})},
              {"config", workspace.config.string()},
              {"invoke", json::array({"cppcheck"})}}}};
        int answerStatus = 0;
        json answer;
        std::thread inFlight(
            [&]
            {
                httplib::Client client("127.0.0.1", port);
                client.set_read_timeout(60, 0);
                if (const auto result = client.Post("/api", request.dump(), "application/json"))
                {
                    answerStatus = result->status;
                    answer = json::parse(result->body, nullptr, /*allow_exceptions=*/false);
                }
            });
        const std::vector<pid_t> tool = workspace.waitForTool();
        server.signal(SIGTERM);
        inFlight.join();
        report.expect(tool.size() == 2 && answerStatus == 200 && answer.is_object() &&
                          answer.value("status", "") == "ok",
                      "server: the analysis in flight when SIGTERM arrives is answered");
        const std::optional<int> status = server.waitEnd(std::chrono::seconds(15));
        report.expect(endedBy(status, SIGTERM), "server: ends by SIGTERM once drained");
        report.expect(workspace.printed().find("Shutdown complete") != std::string::npos,
                      "server: drains as POST /shutdown does");
        killLeftovers(tool);
        std::cout << workspace.printed();
    }
} // namespace

int main(int argc, char* argv[])
{
    if (argc != 4)
    {
        std::cerr << "Usage: ctrace_interrupt_tests <ctrace> <repository root> <work directory>\n";
        return 2;
    }
    const std::string ctrace = argv[1];
    const fs::path repoRoot = argv[2];
    const fs::path work = argv[3];
    TestReport report;

    testInterruptedRun(report, ctrace, repoRoot, work, SIGINT);
    testInterruptedRun(report, ctrace, repoRoot, work, SIGTERM);
    testToolThatEndedFirst(report, ctrace, repoRoot, work, /*oneJob=*/true);
    testToolThatEndedFirst(report, ctrace, repoRoot, work, /*oneJob=*/false);
    testNoToolStartsAfterInterrupt(report, ctrace, repoRoot, work);
    testServerDrains(report, ctrace, repoRoot, work);

    if (report.failures == 0)
    {
        std::cout << "interrupt_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " interrupt check(s) failed\n";
    return 1;
}
