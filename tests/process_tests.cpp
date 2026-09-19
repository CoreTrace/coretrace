// SPDX-License-Identifier: Apache-2.0
//
// Contract of the external process runner: callers always learn how the child ended.
#include "Process/ProcessFactory.hpp"

#include <csignal>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

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

    if (report.failures == 0)
    {
        std::cout << "process_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " process check(s) failed\n";
    return 1;
}
