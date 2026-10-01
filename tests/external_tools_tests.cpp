// SPDX-License-Identifier: Apache-2.0
//
// The external tools that accept many files (cppcheck, flawfinder, tscancode) get them all in
// one run. Usage: ctrace_external_tools_tests <source root>; cppcheck must be installed.
#include "Process/Tools/AnalysisTools.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    using namespace ctrace;

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

    /// The findings of one executeBatch run, in reportedBefore order.
    std::vector<Diagnostic> findings(const IAnalysisTool& tool,
                                     const std::vector<std::string>& files,
                                     const ProgramConfig& config)
    {
        ToolOutput output(nullptr, tool.name(), /*mirrorToConsole=*/false);
        tool.executeBatch(files, config, output);
        std::vector<Diagnostic> found = output.diagnostics();
        std::sort(found.begin(), found.end(), reportedBefore);
        return found;
    }

    bool sameFindings(const std::vector<Diagnostic>& a, const std::vector<Diagnostic>& b)
    {
        return std::equal(
            a.begin(), a.end(), b.begin(), b.end(), [](const Diagnostic& x, const Diagnostic& y)
            { return !reportedBefore(x, y) && !reportedBefore(y, x) && x.cwe == y.cwe; });
    }

    // Analyzing the files together finds what analyzing them one by one finds.
    void testCppcheckFindsTheSameTogether(TestReport& report, const std::string& root)
    {
        const std::vector<std::string> files = {root + "/tests/double_free.c",
                                                root + "/tests/dead_code.cc",
                                                root + "/tests/null_pointer.c"};
        const ProgramConfig config;
        const CppCheckToolImplementation cppcheck;
        std::vector<Diagnostic> oneByOne;
        for (const std::string& file : files)
        {
            const auto found = findings(cppcheck, {file}, config);
            oneByOne.insert(oneByOne.end(), found.begin(), found.end());
        }
        std::sort(oneByOne.begin(), oneByOne.end(), reportedBefore);
        const auto together = findings(cppcheck, files, config);
        report.expect(!together.empty() && sameFindings(together, oneByOne),
                      "cppcheck: the files analyzed together give the findings of each one (" +
                          std::to_string(together.size()) + " vs " +
                          std::to_string(oneByOne.size()) + ")");
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: ctrace_external_tools_tests <source root>\n";
        return 2;
    }
    const std::string root = argv[1];
    TestReport report;
    testCppcheckFindsTheSameTogether(report, root);

    if (report.failures == 0)
    {
        std::cout << "external_tools_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " external tools check(s) failed\n";
    return 1;
}
