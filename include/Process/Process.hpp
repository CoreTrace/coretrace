// SPDX-License-Identifier: Apache-2.0
#ifndef PROCESS_HPP
#define PROCESS_HPP

#include <string>
#include <string_view>
#include <vector>

/// How an external tool ended, plus everything it printed.
struct ProcessResult
{
    int exitCode = -1;  ///< Exit status when the child exited normally; -1 otherwise.
    int signal = 0;     ///< Terminating signal when the child was killed; 0 otherwise.
    std::string output; ///< Combined stdout and stderr, in emission order.

    [[nodiscard]] bool succeeded() const noexcept
    {
        return signal == 0 && exitCode == 0;
    }

    [[nodiscard]] std::string describeFailure(std::string_view toolName) const
    {
        std::string message(toolName);
        if (signal != 0)
        {
            message += " was terminated by signal " + std::to_string(signal);
        }
        else
        {
            message += " exited with exit code " + std::to_string(exitCode);
        }
        return message;
    }
};

/// Runs one external command to completion.
///
/// `execute()` either returns how the child ended or throws `std::runtime_error` when the
/// command cannot be started at all (not found, not executable, spawn failure). Callers must
/// look at the result: a tool that is missing or crashes is never a silent success.
class Process
{
  public:
    virtual ~Process() = default;

    [[nodiscard]] ProcessResult execute()
    {
        prepare();
        ProcessResult result = run();
        cleanup();
        return result;
    }

  protected:
    virtual void prepare() = 0;
    [[nodiscard]] virtual ProcessResult run() = 0;
    virtual void cleanup() = 0;

    std::vector<std::string> m_arguments;
};

#endif // PROCESS_HPP
