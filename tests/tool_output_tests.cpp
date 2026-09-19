// SPDX-License-Identifier: Apache-2.0
//
// ToolOutput is the explicit sink every tool writes its results and errors to: it records
// them for the server response and mirrors them to the console.
#include "Process/Tools/ToolOutput.hpp"

#include <iostream>
#include <memory>
#include <string>

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
} // namespace

int main()
{
    TestReport report;

    auto buffer = std::make_shared<ctrace::CaptureBuffer>();
    {
        ctrace::ToolOutput out(buffer, "cppcheck", /*mirrorToConsole=*/false);
        out.result("line one");
        out.error("something failed");
        out.record("stdout", "captured only");
    }
    const auto snapshot = buffer->snapshot();
    report.expect(snapshot.count("cppcheck") == 1 && snapshot.at("cppcheck").size() == 3,
                  "ToolOutput: three entries recorded under the tool name");
    if (snapshot.count("cppcheck") == 1 && snapshot.at("cppcheck").size() == 3)
    {
        const auto& lines = snapshot.at("cppcheck");
        report.expect(lines[0].stream == "stdout" && lines[0].message == "line one",
                      "ToolOutput: result() is recorded once as stdout");
        report.expect(lines[1].stream == "stderr" && lines[1].message == "something failed",
                      "ToolOutput: error() is recorded as stderr");
        report.expect(lines[2].stream == "stdout" && lines[2].message == "captured only",
                      "ToolOutput: record() captures without printing");
    }

    {
        ctrace::ToolOutput detached(nullptr, "flawfinder", /*mirrorToConsole=*/false);
        detached.result("no buffer");
        detached.error("no buffer either");
        report.expect(true, "ToolOutput: works without a capture buffer");
    }

    if (report.failures == 0)
    {
        std::cout << "tool_output_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " tool output check(s) failed\n";
    return 1;
}
