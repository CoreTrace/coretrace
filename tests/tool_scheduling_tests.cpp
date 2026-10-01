// SPDX-License-Identifier: Apache-2.0
//
// How the invoker schedules tool runs, and what does not depend on it: the findings it reports
// are the same, in the same order, whatever order the runs finish in.
#include "Process/Tools/Sarif.hpp"
#include "Process/Tools/ToolsInvoker.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
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

    /// The runs a tool was asked for, as `tool:file` (or `tool:[a,b]` for a batch run), in the
    /// order they started.
    struct RunLog
    {
        std::mutex mutex;
        std::vector<std::string> runs;

        void add(std::string run)
        {
            const std::lock_guard<std::mutex> lock(mutex);
            runs.push_back(std::move(run));
        }
    };

    class LoggingTool : public ctrace::AnalysisToolBase
    {
      public:
        LoggingTool(std::string name, std::shared_ptr<RunLog> log, bool batch)
            : m_name(std::move(name)), m_log(std::move(log)), m_batch(batch)
        {
        }

        void execute(const std::string& file, const ctrace::ProgramConfig&,
                     ctrace::ToolOutput&) const override
        {
            m_log->add(m_name + ":" + file);
        }

        void executeBatch(const std::vector<std::string>& files, const ctrace::ProgramConfig&,
                          ctrace::ToolOutput&) const override
        {
            m_log->add(m_name + ":[" + ctrace_tools::strings::joinByComma(files) + "]");
        }

        [[nodiscard]] bool supportsBatchExecution() const override
        {
            return m_batch;
        }

        std::string name() const override
        {
            return m_name;
        }

      private:
        std::string m_name;
        std::shared_ptr<RunLog> m_log;
        bool m_batch;
    };

    using Clock = std::chrono::steady_clock;

    /// When each file's run started and ended.
    struct Timeline
    {
        std::mutex mutex;
        std::map<std::string, std::pair<Clock::time_point, Clock::time_point>> runs;

        [[nodiscard]] bool anyOverlap()
        {
            const std::lock_guard<std::mutex> lock(mutex);
            for (const auto& [file, run] : runs)
            {
                for (const auto& [other, otherRun] : runs)
                {
                    if (file < other && run.first < otherRun.second && otherRun.first < run.second)
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        [[nodiscard]] Clock::time_point end(const std::string& file)
        {
            const std::lock_guard<std::mutex> lock(mutex);
            return runs.at(file).second;
        }
    };

    /// Takes a while on each file, much longer on slow.c, records when, and reports one
    /// finding per file.
    class TimedTool : public ctrace::AnalysisToolBase
    {
      public:
        explicit TimedTool(std::shared_ptr<Timeline> timeline) : m_timeline(std::move(timeline)) {}

        void execute(const std::string& file, const ctrace::ProgramConfig&,
                     ctrace::ToolOutput& output) const override
        {
            const Clock::time_point start = Clock::now();
            std::this_thread::sleep_for(file == "slow.c" ? std::chrono::milliseconds(1000)
                                                         : std::chrono::milliseconds(150));
            {
                const std::lock_guard<std::mutex> lock(m_timeline->mutex);
                m_timeline->runs[file] = {start, Clock::now()};
            }
            output.diagnostics(
                {{"timed", "rule", file, 1, 1, ctrace::Severity::Warning, "finding", ""}});
        }

        std::string name() const override
        {
            return "timed";
        }

      private:
        std::shared_ptr<Timeline> m_timeline;
    };

    /// Takes a while on each file, records when, and allows one run at a time.
    class OneAtATimeTool : public ctrace::AnalysisToolBase
    {
      public:
        explicit OneAtATimeTool(std::shared_ptr<Timeline> timeline)
            : m_timeline(std::move(timeline))
        {
        }

        void execute(const std::string& file, const ctrace::ProgramConfig&,
                     ctrace::ToolOutput&) const override
        {
            const Clock::time_point start = Clock::now();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            const std::lock_guard<std::mutex> lock(m_timeline->mutex);
            m_timeline->runs[file] = {start, Clock::now()};
        }

        [[nodiscard]] std::size_t maxConcurrentRuns() const override
        {
            return 1;
        }

        std::string name() const override
        {
            return "one_at_a_time";
        }

      private:
        std::shared_ptr<Timeline> m_timeline;
    };

    std::vector<ctrace::Diagnostic> runTimedTool(std::launch policy, std::size_t workers,
                                                 const std::vector<std::string>& files,
                                                 const std::shared_ptr<Timeline>& timeline)
    {
        ctrace::ToolInvoker invoker(ctrace::ProgramConfig{}, workers, policy);
        invoker.registerTool("timed", std::make_unique<TimedTool>(timeline));
        invoker.runSpecificTools({"timed"}, files);
        return invoker.diagnostics();
    }

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

    // A sequential run goes file by file, each file through the per-file tools in the requested
    // order, then each batch tool once over every file.
    void testSequentialRunOrder(TestReport& report)
    {
        auto log = std::make_shared<RunLog>();
        ctrace::ToolInvoker invoker(ctrace::ProgramConfig{}, 4, std::launch::deferred);
        invoker.registerTool("second", std::make_unique<LoggingTool>("second", log, false));
        invoker.registerTool("first", std::make_unique<LoggingTool>("first", log, false));
        invoker.registerTool("batch", std::make_unique<LoggingTool>("batch", log, true));
        invoker.runSpecificTools({"batch", "second", "first"}, {"b.c", "a.c"});
        report.expect(
            (log->runs == std::vector<std::string>{"second:b.c", "first:b.c", "second:a.c",
                                                   "first:a.c", "batch:[b.c,a.c]"}),
            "a sequential run goes file by file, then runs the batch tools");
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

    // On the pool, one tool analyzes several files at the same time.
    void testFilesRunAtTheSameTime(TestReport& report)
    {
        auto timeline = std::make_shared<Timeline>();
        (void)runTimedTool(std::launch::async, 4, {"a.c", "b.c", "c.c", "d.c"}, timeline);
        report.expect(timeline->anyOverlap(), "an async run analyzes different files at once");

        auto sequential = std::make_shared<Timeline>();
        (void)runTimedTool(std::launch::deferred, 4, {"a.c", "b.c"}, sequential);
        report.expect(!sequential->anyOverlap(), "a sequential run analyzes one file at a time");
    }

    // A file that takes long holds one worker; the other files go through the others.
    void testSlowFileDoesNotBlockTheOthers(TestReport& report)
    {
        const std::vector<std::string> files = {"slow.c", "a.c", "b.c", "c.c"};
        auto timeline = std::make_shared<Timeline>();
        const auto async = runTimedTool(std::launch::async, 2, files, timeline);
        const Clock::time_point slowEnd = timeline->end("slow.c");
        report.expect(timeline->end("a.c") < slowEnd && timeline->end("b.c") < slowEnd &&
                          timeline->end("c.c") < slowEnd,
                      "the other files are analyzed while a slow file is");

        const auto sequential =
            runTimedTool(std::launch::deferred, 2, files, std::make_shared<Timeline>());
        report.expect(ctrace::renderSarif(async) == ctrace::renderSarif(sequential),
                      "the SARIF document is the same when files finish out of order");
    }

    // A tool that allows one run at a time gets one, and does not keep the other tools waiting.
    void testToolConcurrencyLimit(TestReport& report)
    {
        auto limited = std::make_shared<Timeline>();
        auto timed = std::make_shared<Timeline>();
        ctrace::ToolInvoker invoker(ctrace::ProgramConfig{}, 2, std::launch::async);
        invoker.registerTool("one_at_a_time", std::make_unique<OneAtATimeTool>(limited));
        invoker.registerTool("timed", std::make_unique<TimedTool>(timed));
        invoker.runSpecificTools({"one_at_a_time", "timed"}, {"a.c", "b.c", "c.c"});

        report.expect(!limited->anyOverlap(), "a tool limited to one run never runs twice at once");
        const Clock::time_point limitedEnd = limited->end("c.c");
        report.expect(timed->end("a.c") < limitedEnd && timed->end("b.c") < limitedEnd &&
                          timed->end("c.c") < limitedEnd,
                      "the other tools run while the limited tool works through its files");
    }
} // namespace

int main()
{
    TestReport report;
    testSequentialRunOrder(report);
    testToolOrderIsFixed(report);
    testFindingsAreSortedByPosition(report);
    testSarifDoesNotDependOnScheduling(report);
    testFilesRunAtTheSameTime(report);
    testSlowFileDoesNotBlockTheOthers(report);
    testToolConcurrencyLimit(report);

    if (report.failures == 0)
    {
        std::cout << "tool_scheduling_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " tool scheduling check(s) failed\n";
    return 1;
}
