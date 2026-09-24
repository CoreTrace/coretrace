// SPDX-License-Identifier: Apache-2.0
//
// The invoker hands each file only to the tools that analyze its language: a C/C++ tool never
// sees a .py file and a Python tool never sees a .c file, in per-file and batch execution,
// synchronously and on the thread pool.
#include "Process/Tools/ToolsInvoker.hpp"

#include <algorithm>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
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

    struct Seen
    {
        std::mutex mutex;
        std::vector<std::string> files;

        std::vector<std::string> sorted()
        {
            std::lock_guard<std::mutex> lock(mutex);
            std::vector<std::string> copy = files;
            std::sort(copy.begin(), copy.end());
            return copy;
        }
    };

    /// Records the files it is given. Analyzes C/C++ unless told it is a Python tool.
    class RecordingTool : public ctrace::AnalysisToolBase
    {
      public:
        RecordingTool(std::string name, std::shared_ptr<Seen> seen, bool python, bool batch)
            : m_name(std::move(name)), m_seen(std::move(seen)), m_python(python), m_batch(batch)
        {
        }

        void execute(const std::string& file, const ctrace::ProgramConfig&,
                     ctrace::ToolOutput&) const override
        {
            std::lock_guard<std::mutex> lock(m_seen->mutex);
            m_seen->files.push_back(file);
        }

        [[nodiscard]] bool supportsBatchExecution() const override
        {
            return m_batch;
        }

        [[nodiscard]] bool analyzes(ctrace_defs::LanguageType language) const override
        {
            if (m_python)
            {
                return language == ctrace_defs::LanguageType::Python;
            }
            return AnalysisToolBase::analyzes(language);
        }

        std::string name() const override
        {
            return m_name;
        }

      private:
        std::string m_name;
        std::shared_ptr<Seen> m_seen;
        bool m_python;
        bool m_batch;
    };

    void checkRouting(TestReport& report, std::launch policy, bool batch, const std::string& label)
    {
        ctrace::ToolInvoker invoker(ctrace::ProgramConfig{}, 2, policy);
        auto cSeen = std::make_shared<Seen>();
        auto pySeen = std::make_shared<Seen>();
        invoker.registerTool("fake_c",
                             std::make_unique<RecordingTool>("fake_c", cSeen, false, batch));
        invoker.registerTool("fake_py",
                             std::make_unique<RecordingTool>("fake_py", pySeen, true, batch));

        invoker.runSpecificTools({"fake_c", "fake_py"}, {"a.c", "b.py", "c.cpp", "d/e.py"});

        report.expect((cSeen->sorted() == std::vector<std::string>{"a.c", "c.cpp"}),
                      label + ": the C/C++ tool gets only C and C++ files");
        report.expect((pySeen->sorted() == std::vector<std::string>{"b.py", "d/e.py"}),
                      label + ": the Python tool gets only Python files");

        auto idle = std::make_shared<Seen>();
        ctrace::ToolInvoker onlyC(ctrace::ProgramConfig{}, 2, policy);
        onlyC.registerTool("fake_py",
                           std::make_unique<RecordingTool>("fake_py", idle, true, batch));
        onlyC.runSpecificTools({"fake_py"}, std::vector<std::string>{"a.c"});
        report.expect(idle->sorted().empty() && onlyC.diagnostics().empty() &&
                          onlyC.uninterpretedTools().empty(),
                      label + ": a tool with no file of its language does not run at all");
    }
} // namespace

int main()
{
    TestReport report;
    checkRouting(report, std::launch::deferred, /*batch=*/false, "sync, per file");
    checkRouting(report, std::launch::async, /*batch=*/false, "async, per file");
    checkRouting(report, std::launch::deferred, /*batch=*/true, "sync, batch");

    if (report.failures == 0)
    {
        std::cout << "tool_routing_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " tool routing check(s) failed\n";
    return 1;
}
