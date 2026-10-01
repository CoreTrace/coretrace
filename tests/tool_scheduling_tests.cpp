// SPDX-License-Identifier: Apache-2.0
//
// How the invoker schedules tool runs, and what does not depend on it: the findings it reports
// are the same, in the same order, whatever order the runs finish in.
#include "Process/Tools/Sarif.hpp"
#include "Process/Tools/ToolsInvoker.hpp"

#include <nlohmann/json.hpp>

#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
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

    /// Reports, for each file, one finding per line of `lines`, in that order.
    class ReportingTool : public ctrace::AnalysisToolBase
    {
      public:
        ReportingTool(std::string name, std::vector<unsigned> lines)
            : m_name(std::move(name)), m_lines(std::move(lines))
        {
        }

        void execute(const std::string& file, const ctrace::ProgramConfig&,
                     ctrace::ToolOutput& output) const override
        {
            std::vector<ctrace::Diagnostic> found;
            for (const unsigned line : m_lines)
            {
                found.push_back(
                    {m_name, "rule", file, line, 1, ctrace::Severity::Warning, "finding", ""});
            }
            output.diagnostics(std::move(found));
        }

        std::string name() const override
        {
            return m_name;
        }

      private:
        std::string m_name;
        std::vector<unsigned> m_lines;
    };

    /// `tool:file:line` for each finding, in reported order.
    std::vector<std::string> positions(const std::vector<ctrace::Diagnostic>& diagnostics)
    {
        std::vector<std::string> rendered;
        for (const ctrace::Diagnostic& diagnostic : diagnostics)
        {
            rendered.push_back(diagnostic.tool + ":" + diagnostic.file + ":" +
                               std::to_string(diagnostic.line));
        }
        return rendered;
    }

    std::vector<ctrace::Diagnostic> runReportingTools(std::launch policy,
                                                      const std::vector<std::string>& files)
    {
        ctrace::ToolInvoker invoker(ctrace::ProgramConfig{}, 4, policy);
        invoker.registerTool("zeta", std::make_unique<ReportingTool>("zeta", std::vector{1U}));
        invoker.registerTool("alpha", std::make_unique<ReportingTool>("alpha", std::vector{1U}));
        invoker.runSpecificTools({"zeta", "alpha"}, files);
        return invoker.diagnostics();
    }

    void testToolOrderIsFixed(TestReport& report)
    {
        report.expect((positions(runReportingTools(std::launch::deferred, {"a.c"})) ==
                       std::vector<std::string>{"alpha:a.c:1", "zeta:a.c:1"}),
                      "findings are grouped by tool name, not by the order tools ran in");
    }

    // Within a tool, findings are ordered by file then position, not by the order the files
    // were given in or the order the tool reported them in.
    void testFindingsAreSortedByPosition(TestReport& report)
    {
        ctrace::ToolInvoker invoker(ctrace::ProgramConfig{}, 4, std::launch::deferred);
        invoker.registerTool("tool", std::make_unique<ReportingTool>("tool", std::vector{9U, 2U}));
        invoker.runSpecificTools({"tool"}, {"b.c", "a.c"});
        report.expect(
            (positions(invoker.diagnostics()) ==
             std::vector<std::string>{"tool:a.c:2", "tool:a.c:9", "tool:b.c:2", "tool:b.c:9"}),
            "findings are sorted by file, then line");
    }

    // The merged SARIF document is the report users diff and baseline against.
    void testSarifDoesNotDependOnScheduling(TestReport& report)
    {
        const std::vector<std::string> files = {"c.c", "a.c", "b.c"};
        report.expect(ctrace::renderSarif(runReportingTools(std::launch::deferred, files)) ==
                          ctrace::renderSarif(runReportingTools(std::launch::async, files)),
                      "the SARIF document is the same for a sequential and an async run");
    }
} // namespace

int main()
{
    TestReport report;
    testToolOrderIsFixed(report);
    testFindingsAreSortedByPosition(report);
    testSarifDoesNotDependOnScheduling(report);

    if (report.failures == 0)
    {
        std::cout << "tool_scheduling_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " tool scheduling check(s) failed\n";
    return 1;
}
