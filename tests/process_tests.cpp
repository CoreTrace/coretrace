// SPDX-License-Identifier: Apache-2.0
//
// Contract of the external process runner: callers always learn how the child ended.
#include "Process/ProcessFactory.hpp"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

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

    ProcessResult runShell(const std::string& script)
    {
        auto process = ProcessFactory::createProcess("sh", {"-c", script});
        return process->execute();
    }

    void testExitCodeIsReported(TestReport& report)
    {
        const ProcessResult result = runShell("exit 3");
        report.expect(result.signal == 0, "exit 3: no signal");
        report.expect(result.exitCode == 3, "exit 3: exit code is 3");
        report.expect(!result.succeeded(), "exit 3: not a success");
    }

    void testFalseIsAFailure(TestReport& report)
    {
        auto process = ProcessFactory::createProcess("false");
        const ProcessResult result = process->execute();
        report.expect(result.exitCode == 1 && !result.succeeded(), "false: exit code 1");
    }

    void testOutputIsCaptured(TestReport& report)
    {
        const ProcessResult result = runShell("echo on-stdout; echo on-stderr 1>&2; exit 0");
        report.expect(result.succeeded(), "echo: success");
        report.expect(result.output.find("on-stdout") != std::string::npos,
                      "echo: stdout is captured");
        report.expect(result.output.find("on-stderr") != std::string::npos,
                      "echo: stderr is captured");
    }

    void testSignalIsReported(TestReport& report)
    {
        const ProcessResult result = runShell("kill -TERM $$");
        report.expect(result.signal == SIGTERM, "kill: termination signal is SIGTERM");
        report.expect(!result.succeeded(), "kill: not a success");
    }

    void testMissingCommandIsAnExplicitError(TestReport& report)
    {
        const std::string command = "ctrace-no-such-tool-xyz";
        bool threw = false;
        std::string message;
        try
        {
            auto process = ProcessFactory::createProcess(command);
            (void)process->execute();
        }
        catch (const std::runtime_error& e)
        {
            threw = true;
            message = e.what();
        }
        report.expect(threw, "missing command: execute throws");
        report.expect(message.find(command) != std::string::npos,
                      "missing command: the error names the command (got '" + message + "')");
    }

    void testMissingCommandWithPathIsAnExplicitError(TestReport& report)
    {
        const std::string command = "/nonexistent/dir/ctrace-tool";
        bool threw = false;
        std::string message;
        try
        {
            auto process = ProcessFactory::createProcess(command);
            (void)process->execute();
        }
        catch (const std::runtime_error& e)
        {
            threw = true;
            message = e.what();
        }
        report.expect(threw && message.find(command) != std::string::npos,
                      "missing command with path: explicit error naming the path");
    }

    namespace fs = std::filesystem;

    /// The error execute() throws for `command`; empty when the command runs.
    std::string executeError(const std::string& command)
    {
        try
        {
            auto process = ProcessFactory::createProcess(command);
            (void)process->execute();
        }
        catch (const std::runtime_error& e)
        {
            return e.what();
        }
        return {};
    }

    void writeScript(const fs::path& path, const std::string& body, fs::perms permissions)
    {
        std::ofstream(path) << "#!/bin/sh\n" << body << "\n";
        fs::permissions(path, permissions);
    }

    /// Sets PATH for the scope, and restores the previous value (or its absence) after.
    class ScopedPath
    {
      public:
        explicit ScopedPath(const std::optional<std::string>& value)
        {
            if (const char* current = std::getenv("PATH"))
            {
                saved_ = current;
            }
            if (value)
            {
                ::setenv("PATH", value->c_str(), 1);
            }
            else
            {
                ::unsetenv("PATH");
            }
        }

        ~ScopedPath()
        {
            if (saved_)
            {
                ::setenv("PATH", saved_->c_str(), 1);
            }
            else
            {
                ::unsetenv("PATH");
            }
        }

        ScopedPath(const ScopedPath&) = delete;
        ScopedPath& operator=(const ScopedPath&) = delete;

      private:
        std::optional<std::string> saved_;
    };

    void testUnsetPathIsAnExplicitError(TestReport& report)
    {
        const ScopedPath noPath(std::nullopt);
        const std::string error = executeError("sh");
        report.expect(error.find("PATH is not set") != std::string::npos,
                      "unset PATH: the error says so (got '" + error + "')");
    }

    void testCommandThatCannotRun(TestReport& report, const fs::path& base)
    {
        const fs::path notExecutable = base / "not-executable";
        writeScript(notExecutable, "exit 0", fs::perms::owner_read | fs::perms::owner_write);
        const std::string denied = executeError(notExecutable.string());
        report.expect(denied.find("Command not executable") != std::string::npos,
                      "a file without execute permission is refused (got '" + denied + "')");

        const std::string directory = executeError(base.string() + "/");
        report.expect(directory.find("not a regular file") != std::string::npos,
                      "a directory is not run as a command (got '" + directory + "')");
    }

    // As a shell does: a directory named like the command does not hide the executable that a
    // later PATH entry holds.
    void testPathLookupSkipsDirectories(TestReport& report, const fs::path& base)
    {
        const fs::path shadow = base / "shadow";
        const fs::path bin = base / "bin";
        fs::create_directories(shadow / "ctrace-probe-tool");
        fs::create_directories(bin);
        writeScript(bin / "ctrace-probe-tool", "echo probe-found", fs::perms::owner_all);

        const ScopedPath path(shadow.string() + ":" + bin.string());
        std::string error;
        ProcessResult result;
        try
        {
            result = ProcessFactory::createProcess("ctrace-probe-tool")->execute();
        }
        catch (const std::runtime_error& e)
        {
            error = e.what();
        }
        report.expect(error.empty() && result.succeeded() &&
                          result.output.find("probe-found") != std::string::npos,
                      "PATH lookup skips a directory with the command's name (" +
                          (error.empty() ? result.output : error) + ")");
    }

    void testLargeOutputIsCapturedWhole(TestReport& report)
    {
        const ProcessResult result = runShell("head -c 1000000 /dev/zero | tr '\\000' x");
        report.expect(result.succeeded() && result.output.size() == 1000000,
                      "a megabyte of output is captured whole (got " +
                          std::to_string(result.output.size()) + " bytes)");
    }

    void testArgumentsArePassedVerbatim(TestReport& report)
    {
        auto process = ProcessFactory::createProcess(
            "sh", {"-c", "printf '%s|' \"$@\"", "sh", "two words", "with \"quotes\"", "$HOME"});
        const ProcessResult result = process->execute();
        report.expect(result.output == "two words|with \"quotes\"|$HOME|",
                      "arguments reach the command verbatim, never through a shell (got '" +
                          result.output + "')");
    }

    // A deadline stops a child that runs too long, and everything it started: the whole
    // process group, so that a grandchild holding the output does not outlive it.
    void testDeadlineStopsTheProcessGroup(TestReport& report, const fs::path& base)
    {
        const fs::path pidFile = base / "grandchild.pid";
        const auto start = std::chrono::steady_clock::now();
        auto process = ProcessFactory::createProcess(
            "sh", {"-c", "sleep 30 & echo $! > '" + pidFile.string() + "'; wait"},
            std::chrono::seconds(1));
        const ProcessResult result = process->execute();
        const auto elapsed = std::chrono::steady_clock::now() - start;

        report.expect(result.timedOut && !result.succeeded(),
                      "deadline: the child is reported as timed out");
        report.expect(elapsed < std::chrono::seconds(10),
                      "deadline: execute returns soon after the deadline");
        report.expect(result.describeFailure("tool").find("time limit") != std::string::npos,
                      "deadline: the failure names the time limit (" +
                          result.describeFailure("tool") + ")");

        std::ifstream in(pidFile);
        pid_t grandchild = 0;
        in >> grandchild;
        // Killed, the grandchild stays a zombie until init reaps it, and kill(pid, 0) still
        // succeeds on a zombie: give it a moment to go away.
        bool gone = false;
        for (int attempt = 0; grandchild > 0 && attempt < 100 && !gone; ++attempt)
        {
            gone = ::kill(grandchild, 0) != 0;
            if (!gone)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        report.expect(gone, "deadline: the grandchild is stopped with it");
    }

    void testFastChildIsUnaffectedByADeadline(TestReport& report)
    {
        auto process =
            ProcessFactory::createProcess("sh", {"-c", "echo done"}, std::chrono::seconds(30));
        const ProcessResult result = process->execute();
        report.expect(result.succeeded() && !result.timedOut &&
                          result.output.find("done") != std::string::npos,
                      "deadline: a child that ends in time is a normal run");
    }

    void testDescribeFailure(TestReport& report)
    {
        ProcessResult exited;
        exited.exitCode = 2;
        report.expect(exited.describeFailure("cppcheck").find("exit code 2") != std::string::npos,
                      "describeFailure: mentions the exit code");
        report.expect(exited.describeFailure("cppcheck").find("cppcheck") != std::string::npos,
                      "describeFailure: mentions the tool name");

        ProcessResult killed;
        killed.exitCode = -1;
        killed.signal = SIGKILL;
        report.expect(killed.describeFailure("ikos").find("signal") != std::string::npos,
                      "describeFailure: mentions the signal");
    }
} // namespace

int main()
{
    TestReport report;
    testExitCodeIsReported(report);
    testFalseIsAFailure(report);
    testOutputIsCaptured(report);
    testSignalIsReported(report);
    testMissingCommandIsAnExplicitError(report);
    testMissingCommandWithPathIsAnExplicitError(report);
    testDescribeFailure(report);

    const fs::path base =
        fs::temp_directory_path() / ("ctrace-process-" + std::to_string(::getpid()));
    fs::remove_all(base);
    fs::create_directories(base);
    testUnsetPathIsAnExplicitError(report);
    testCommandThatCannotRun(report, base);
    testPathLookupSkipsDirectories(report, base);
    testLargeOutputIsCapturedWhole(report);
    testArgumentsArePassedVerbatim(report);
    testDeadlineStopsTheProcessGroup(report, base);
    testFastChildIsUnaffectedByADeadline(report);
    fs::remove_all(base);

    if (report.failures == 0)
    {
        std::cout << "process_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " process check(s) failed\n";
    return 1;
}
