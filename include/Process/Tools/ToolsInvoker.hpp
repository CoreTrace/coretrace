// SPDX-License-Identifier: Apache-2.0
#ifndef TOOLS_INVOKER_HPP
#define TOOLS_INVOKER_HPP

#include "AnalysisTools.hpp"
#include "App/ExitPolicy.hpp"
#include "Process/Ipc/IpcStrategy.hpp"

#include <coretrace/logger.hpp>

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if __has_include(<version>)
#include <version>
#endif

class ThreadPool
{
  public:
    explicit ThreadPool(std::size_t numThreads) : stopping(false)
    {
        if (numThreads == 0)
        {
            numThreads = 1;
        }

        workers.reserve(numThreads);
        for (size_t i = 0; i < numThreads; ++i)
        {
            workers.emplace_back(
                [this]
                {
                    while (true)
                    {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(queueMutex);
                            condition.wait(lock, [this] { return stopping || !tasks.empty(); });
                            if (stopping && tasks.empty())
                                return;
                            task = std::move(tasks.front());
                            tasks.pop();
                        }
                        task();
                    }
                });
        }
    }

    ~ThreadPool()
    {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            stopping = true;
        }
        condition.notify_all();
        for (auto& worker : workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
    }

    template <typename F> auto enqueue(F&& f) -> std::future<std::invoke_result_t<std::decay_t<F>>>
    {
        using return_type = std::invoke_result_t<std::decay_t<F>>;
        auto task = std::make_shared<std::packaged_task<return_type()>>(std::forward<F>(f));

        std::future<return_type> res = task->get_future();
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (stopping)
                throw std::runtime_error("Enqueue on stopped ThreadPool");
            tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return res;
    }

  private:
#if defined(__cpp_lib_jthread) && (__cpp_lib_jthread >= 201911L)
    using WorkerThread = std::jthread;
#else
    using WorkerThread = std::thread;
#endif
    std::vector<WorkerThread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stopping;
};

namespace ctrace
{
    class ToolInvoker
    {
      public:
        ToolInvoker(ctrace::ProgramConfig config, std::size_t nbThreadPool, std::launch policy,
                    std::shared_ptr<ctrace::CaptureBuffer> output_capture = nullptr)
            : m_config(std::move(config)), m_nbThreadPool(nbThreadPool == 0 ? 1 : nbThreadPool),
              m_policy(policy), m_output_capture(output_capture)
        {
            coretrace::log(coretrace::Level::Debug, "Initializing ToolInvoker...\n");

            registerTool("cppcheck", std::make_unique<CppCheckToolImplementation>());
            registerTool("flawfinder", std::make_unique<FlawfinderToolImplementation>());
            registerTool("tscancode", std::make_unique<TscancodeToolImplementation>());
            registerTool("ikos", std::make_unique<IkosToolImplementation>());
            registerTool("ctrace_stack_analyzer",
                         std::make_unique<StackAnalyzerToolImplementation>());
            registerTool("coretrace-concurrency-analyzer",
                         std::make_unique<ConcurrencyAnalyzerToolImplementation>());
            registerTool("coretrace-python-analyzer",
                         std::make_unique<PythonAnalyzerToolImplementation>());
            registerTool("coretrace-runtime-analyzer",
                         std::make_unique<RuntimeAnalyzerToolImplementation>());

            static_tools = {"cppcheck",
                            "flawfinder",
                            "tscancode",
                            "ikos",
                            "ctrace_stack_analyzer",
                            "coretrace-concurrency-analyzer",
                            "coretrace-python-analyzer"};
            dynamic_tools = {"coretrace-runtime-analyzer"};

            if (m_config.runtime.ipc == "standardIO")
            {
                m_ipc = nullptr; // Results go to the ToolOutput sink.
                coretrace::log(coretrace::Level::Debug, "Using standardIO for IPC.\n");
            }
            else
            {
                if (const std::string notice =
                        ctrace_defs::ipcDeprecationNotice(m_config.runtime.ipc);
                    !notice.empty())
                {
                    coretrace::log(coretrace::Level::Warn, "{}\n", notice);
                }
                m_ipc = std::make_shared<UnixSocketStrategy>(m_config.runtime.ipc_path);

                for (auto& [_, tool] : tools)
                {
                    tool->setIpcStrategy(m_ipc);
                }
            }

            if (m_policy == std::launch::async)
            {
                m_threadPool = std::make_unique<ThreadPool>(m_nbThreadPool);
                coretrace::log(coretrace::Level::Debug,
                               "ToolInvoker thread pool enabled with {} workers.\n",
                               m_nbThreadPool);
            }
        }

        // Execute all static analysis tools
        void runStaticTools(const std::vector<std::string>& files)
        {
            runToolList(static_tools, files);
        }

        // Execute all dynamic analysis tools
        void runDynamicTools(const std::vector<std::string>& files)
        {
            runToolList(dynamic_tools, files);
        }

        // Execute a specific tool list
        void runSpecificTools(const std::vector<std::string>& tool_names,
                              const std::vector<std::string>& files)
        {
            runToolList(deduplicateToolNames(tool_names), files);
        }

        /// Every finding collected so far, in reportedBefore order: reports do not depend on
        /// the order tool runs finished in.
        [[nodiscard]] std::vector<Diagnostic> diagnostics() const
        {
            std::lock_guard<std::mutex> lock(m_diagnosticsMutex);
            std::vector<Diagnostic> all;
            for (const auto& [_, collected] : m_diagnosticsByTool)
            {
                all.insert(all.end(), collected.items.begin(), collected.items.end());
            }
            std::stable_sort(all.begin(), all.end(), reportedBefore);
            return all;
        }

        [[nodiscard]] DiagnosticSummary diagnosticsSummaryTotal() const
        {
            return summarize(diagnostics());
        }

        /// Tools that could not be started, crashed or exited abnormally at least once.
        [[nodiscard]] std::vector<std::string> failedTools() const
        {
            std::lock_guard<std::mutex> lock(m_diagnosticsMutex);
            std::vector<std::string> names;
            for (const auto& [name, collected] : m_diagnosticsByTool)
            {
                if (collected.failed)
                {
                    names.push_back(name);
                }
            }
            return names;
        }

        /// The run's verdict input: every finding counted, and whether any tool failed.
        [[nodiscard]] AnalysisOutcome outcome() const
        {
            return {diagnosticsSummaryTotal(), !failedTools().empty()};
        }

        /// Tools that ran without ever reporting structured findings. Their output may hold
        /// findings the counters do not see; this is distinct from a tool with zero findings.
        [[nodiscard]] std::vector<std::string> uninterpretedTools() const
        {
            std::lock_guard<std::mutex> lock(m_diagnosticsMutex);
            std::vector<std::string> names;
            for (const auto& [name, collected] : m_diagnosticsByTool)
            {
                if (!collected.interpreted)
                {
                    names.push_back(name);
                }
            }
            return names;
        }

        /// Adds or replaces the tool run under `name`. Must be called before any run.
        void registerTool(const std::string& name, std::unique_ptr<IAnalysisTool> tool)
        {
            tools[name] = std::move(tool);
        }

      private:
        void executeTool(const std::string& tool_name, const std::string& file)
        {
            runTool(tool_name, [&](const IAnalysisTool& tool, ToolOutput& output)
                    { tool.execute(file, m_config, output); });
        }

        void executeBatchTool(const std::string& tool_name, const std::vector<std::string>& files)
        {
            if (files.empty())
            {
                return;
            }
            runTool(tool_name, [&](const IAnalysisTool& tool, ToolOutput& output)
                    { tool.executeBatch(files, m_config, output); });
        }

        /// Runs `tool_name` and records what it reported. A tool that throws failed, like one
        /// that could not start: its error goes to its sink, the run is incomplete, and the
        /// other tools still run. Runs of one tool may go on at the same time.
        template <typename Execute> void runTool(const std::string& tool_name, Execute&& execute)
        {
            auto tool_it = tools.find(tool_name);
            if (tool_it == tools.end())
            {
                coretrace::log(coretrace::Level::Error, "Unknown tool: {}\n", tool_name);
                return;
            }

            ToolOutput output(m_output_capture, tool_name, /*mirrorToConsole=*/true);
            try
            {
                execute(*tool_it->second, output);
            }
            catch (const std::exception& e)
            {
                output.error(e.what());
            }
            output.flush();
            recordDiagnostics(tool_name, output);
        }

        /// One tool run: a per-file tool on one file, or a batch tool on all of its files.
        struct Job
        {
            std::string tool;
            std::vector<std::string> files;
            bool batch = false;
        };

        /// Runs every tool of `tool_names` over the files of its language. Sequentially, the
        /// runs go in planJobs order; on the pool, they all start as soon as a worker and their
        /// tool allow, so that files are analyzed at the same time and a slow run holds one
        /// worker, not the others.
        void runToolList(const std::vector<std::string>& tool_names,
                         const std::vector<std::string>& files)
        {
            const std::vector<Job> jobs = planJobs(tool_names, files);
            if (m_policy != std::launch::async || !m_threadPool)
            {
                for (const Job& job : jobs)
                {
                    runJob(job);
                }
                return;
            }

            runOnPool(jobs);
        }

        /// The jobs of one tool not started yet, taken in order by the tool's lanes.
        struct ToolQueue
        {
            std::mutex mutex;
            std::deque<const Job*> pending;
            std::size_t lanes = 0;

            [[nodiscard]] const Job* next()
            {
                const std::lock_guard<std::mutex> lock(mutex);
                if (pending.empty())
                {
                    return nullptr;
                }
                const Job* job = pending.front();
                pending.pop_front();
                return job;
            }
        };

        /// Each tool gets up to maxConcurrentRuns lanes, each one pool task that runs the
        /// tool's next job until none is left. A tool at its limit therefore holds no worker
        /// waiting for its turn: workers only ever run jobs.
        void runOnPool(const std::vector<Job>& jobs)
        {
            std::map<std::string, ToolQueue> queues;
            for (const Job& job : jobs)
            {
                queues[job.tool].pending.push_back(&job);
            }
            for (auto& [tool_name, queue] : queues)
            {
                const std::size_t limit = tools.at(tool_name)->maxConcurrentRuns();
                queue.lanes = std::min(queue.pending.size(), limit == 0 ? m_nbThreadPool : limit);
            }

            std::vector<std::future<void>> lanes;
            // Interleaved across tools, so that every tool starts before one gets a second lane.
            for (std::size_t lane = 0;; ++lane)
            {
                bool added = false;
                for (auto& [_, queue] : queues)
                {
                    if (lane < queue.lanes)
                    {
                        added = true;
                        lanes.push_back(m_threadPool->enqueue(
                            [this, &queue]
                            {
                                while (const Job* job = queue.next())
                                {
                                    runJob(*job);
                                }
                            }));
                    }
                }
                if (!added)
                {
                    break;
                }
            }
            // Every lane refers to `queues` and `jobs`: all of them end before an error is
            // rethrown.
            for (auto& lane : lanes)
            {
                lane.wait();
            }
            for (auto& lane : lanes)
            {
                lane.get();
            }
        }

        /// The runs of `tool_names` over `files`, in the order a sequential run takes them:
        /// file by file through the per-file tools, in the requested order, then each batch
        /// tool once over its files. Unknown tools are reported and left out.
        [[nodiscard]] std::vector<Job> planJobs(const std::vector<std::string>& tool_names,
                                                const std::vector<std::string>& files) const
        {
            std::vector<std::string> perFileTools;
            std::vector<Job> batchJobs;
            for (const auto& tool_name : tool_names)
            {
                const auto tool_it = tools.find(tool_name);
                if (tool_it == tools.end())
                {
                    coretrace::log(coretrace::Level::Error, "Unknown tool: {}\n", tool_name);
                    continue;
                }
                if (!tool_it->second->supportsBatchExecution())
                {
                    perFileTools.push_back(tool_name);
                    continue;
                }
                std::vector<std::string> selected = filesAnalyzedBy(*tool_it->second, files);
                if (!selected.empty())
                {
                    batchJobs.push_back({tool_name, std::move(selected), true});
                }
            }

            std::vector<Job> jobs;
            for (const auto& file : files)
            {
                const ctrace_defs::LanguageType language = ctrace_tools::detectLanguage(file);
                for (const auto& tool_name : perFileTools)
                {
                    if (tools.at(tool_name)->analyzes(language))
                    {
                        jobs.push_back({tool_name, {file}, false});
                    }
                }
            }
            jobs.insert(jobs.end(), std::make_move_iterator(batchJobs.begin()),
                        std::make_move_iterator(batchJobs.end()));
            return jobs;
        }

        void runJob(const Job& job)
        {
            if (job.batch)
            {
                executeBatchTool(job.tool, job.files);
            }
            else
            {
                executeTool(job.tool, job.files.front());
            }
        }

        /// The files of `files` whose language `tool` analyzes.
        [[nodiscard]] static std::vector<std::string>
        filesAnalyzedBy(const IAnalysisTool& tool, const std::vector<std::string>& files)
        {
            std::vector<std::string> selected;
            for (const auto& file : files)
            {
                if (tool.analyzes(ctrace_tools::detectLanguage(file)))
                {
                    selected.push_back(file);
                }
            }
            return selected;
        }

        static std::vector<std::string>
        deduplicateToolNames(const std::vector<std::string>& tool_names)
        {
            std::vector<std::string> deduped;
            deduped.reserve(tool_names.size());

            std::unordered_set<std::string> seen;
            seen.reserve(tool_names.size());

            for (const auto& tool_name : tool_names)
            {
                if (seen.insert(tool_name).second)
                {
                    deduped.push_back(tool_name);
                }
            }

            return deduped;
        }

        struct CollectedDiagnostics
        {
            std::vector<Diagnostic> items;
            bool interpreted = false;
            bool failed = false;
        };

        void recordDiagnostics(const std::string& tool_name, const ToolOutput& output)
        {
            if (output.interpreted())
            {
                const DiagnosticSummary summary = summarize(output.diagnostics());
                coretrace::log(coretrace::Level::Info, coretrace::Module(tool_name),
                               "Diagnostics summary: info={}, warning={}, error={}\n", summary.info,
                               summary.warning, summary.error);
            }
            else
            {
                coretrace::log(coretrace::Level::Info, coretrace::Module(tool_name),
                               "Diagnostics summary: not available (tool output was not "
                               "interpreted)\n");
            }

            std::lock_guard<std::mutex> lock(m_diagnosticsMutex);
            CollectedDiagnostics& collected = m_diagnosticsByTool[tool_name];
            collected.interpreted = collected.interpreted || output.interpreted();
            collected.failed = collected.failed || output.failed();
            collected.items.insert(collected.items.end(), output.diagnostics().begin(),
                                   output.diagnostics().end());
        }

        std::unordered_map<std::string, std::unique_ptr<IAnalysisTool>> tools;
        std::vector<std::string> static_tools;
        std::vector<std::string> dynamic_tools;
        ctrace::ProgramConfig m_config;
        std::size_t m_nbThreadPool;
        std::launch m_policy;
        std::shared_ptr<IpcStrategy> m_ipc;
        std::shared_ptr<ctrace::CaptureBuffer> m_output_capture;
        std::unique_ptr<ThreadPool> m_threadPool;
        mutable std::mutex m_diagnosticsMutex;
        std::map<std::string, CollectedDiagnostics> m_diagnosticsByTool;
    };
} // namespace ctrace

#endif // TOOLS_INVOKER_HPP
