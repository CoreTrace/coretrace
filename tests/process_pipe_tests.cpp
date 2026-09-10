// SPDX-License-Identifier: Apache-2.0
//
// A child writing more than one pipe buffer must not hang the parent.
//
// The Windows implementation waited for the child to exit before reading the
// pipe. A pipe holds about 4 KiB, so any tool with more to say than that
// filled it, blocked in its own write, and waited for a parent that was
// waiting for it. cppcheck crosses 4 KiB on ordinary source files, which is
// why a local analysis stopped forever on some files and not others.

#include "Process/ProcessFactory.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <string>

namespace
{
    // Written by the child. Comfortably more than one pipe buffer.
    constexpr int kChildLines = 4000;

    int emitChildOutput()
    {
        for (int line = 0; line < kChildLines; ++line)
        {
            std::cout << "line " << line << " of noisy tool output\n";
        }
        std::cout.flush();
        return 0;
    }

    int failures = 0;

    void check(bool condition, const std::string& what)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << what << "\n";
            ++failures;
            return;
        }
        std::cerr << "ok: " << what << "\n";
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc > 1 && std::string(argv[1]) == "--emit")
    {
        return emitChildOutput();
    }

    auto process = ProcessFactory::createProcess(argv[0], {"--emit"});

    const auto started = std::chrono::steady_clock::now();
    try
    {
        process->execute();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: execute threw: " << error.what() << "\n";
        return 1;
    }
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started)
            .count();
    std::cerr << "execute returned after " << seconds << "s with "
              << process->logOutput.size() << " bytes\n";

    check(seconds < 30, "a noisy child finishes instead of deadlocking");
    check(process->logOutput.find("line 0 of") != std::string::npos,
          "the first line is captured");
    check(process->logOutput.find("line " + std::to_string(kChildLines - 1) + " of") !=
              std::string::npos,
          "the last line is captured, so nothing was lost past the buffer");

    return failures == 0 ? 0 : 1;
}
