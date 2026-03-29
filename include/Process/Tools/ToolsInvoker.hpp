// SPDX-License-Identifier: Apache-2.0
#ifndef TOOLS_INVOKER_HPP
#define TOOLS_INVOKER_HPP

#include "AnalysisTools.hpp"
#include "Process/Ipc/IpcStrategy.hpp"

#include <coretrace/logger.hpp>

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
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
                    std::shared_ptr<ctrace::Thread::Output::CaptureBuffer> output_capture = nullptr)
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
            registerTool("dyn_tools_1", std::make_unique<DynTool1>());
            registerTool("dyn_tools_2", std::make_unique<DynTool2>());
            registerTool("dyn_tools_3", std::make_unique<DynTool3>());

            static_tools = {"cppcheck", "flawfinder", "tscancode", "ikos", "ctrace_stack_analyzer"};
            dynamic_tools = {"dyn_tools_1", "dyn_tools_2", "dyn_tools_3"};

            if (m_config.global.ipc == "standardIO")
            {
                m_ipc = nullptr; // Use std::cout directely
                coretrace::log(coretrace::Level::Debug, "Using standardIO for IPC.\n");
            }
            else
            {
                m_ipc = std::make_shared<UnixSocketStrategy>(m_config.global.ipcPath);

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
        void runStaticTools(const std::string& file)
        {
            runStaticTools(std::vector<std::string>{file});
        }

        void runStaticTools(const std::vector<std::string>& files)
        {
            runToolList(static_tools, files);
        }

        // Execute all dynamic analysis tools
        void runDynamicTools(const std::string& file)
        {
            runDynamicTools(std::vector<std::string>{file});
        }

        void runDynamicTools(const std::vector<std::string>& files)
        {
            runToolList(dynamic_tools, files);
        }

        // Execute a specific tool list
        void runSpecificTools(const std::vector<std::string>& tool_names, const std::string& file)
        {
            runSpecificTools(tool_names, std::vector<std::string>{file});
        }

        void runSpecificTools(const std::vector<std::string>& tool_names,
                              const std::vector<std::string>& files)
        {
            runToolList(deduplicateToolNames(tool_names), files);
        }

        [[nodiscard]] DiagnosticSummary diagnosticsSummaryTotal() const
        {
            std::lock_guard<std::mutex> lock(m_diagnosticsSummaryMutex);

            DiagnosticSummary total{};
            for (const auto& [_, summary] : m_diagnosticsSummaryByTool)
            {
                total.info += summary.info;
                total.warning += summary.warning;
                total.error += summary.error;
            }

            return total;
        }

      private:
        void registerTool(const std::string& name, std::unique_ptr<IAnalysisTool> tool)
        {
            toolLocks[name] = std::make_shared<std::mutex>();
            tools[name] = std::move(tool);
        }

        void executeTool(const std::string& tool_name, const std::string& file)
        {
            ctrace::Thread::Output::CaptureContext ctx{m_output_capture, tool_name, true};
            ctrace::Thread::Output::ScopedCapture capture(m_output_capture ? &ctx : nullptr);

            auto tool_it = tools.find(tool_name);
            if (tool_it == tools.end())
            {
                ctrace::Thread::Output::cerr("\033[31mUnknown tool: " + tool_name + "\033[0m");
                return;
            }

            auto lock_it = toolLocks.find(tool_name);
            if (lock_it != toolLocks.end() && lock_it->second)
            {
                std::lock_guard<std::mutex> lock(*lock_it->second);
                tool_it->second->execute(file, m_config);
                recordDiagnosticsSummary(tool_name, *tool_it->second);
                return;
            }

            tool_it->second->execute(file, m_config);
            recordDiagnosticsSummary(tool_name, *tool_it->second);
        }

        void executeBatchTool(const std::string& tool_name, const std::vector<std::string>& files)
        {
            if (files.empty())
            {
                return;
            }

            ctrace::Thread::Output::CaptureContext ctx{m_output_capture, tool_name, true};
            ctrace::Thread::Output::ScopedCapture capture(m_output_capture ? &ctx : nullptr);

            auto tool_it = tools.find(tool_name);
            if (tool_it == tools.end())
            {
                ctrace::Thread::Output::cerr("\033[31mUnknown tool: " + tool_name + "\033[0m");
                return;
            }

            auto lock_it = toolLocks.find(tool_name);
            if (lock_it != toolLocks.end() && lock_it->second)
            {
                std::lock_guard<std::mutex> lock(*lock_it->second);
                tool_it->second->executeBatch(files, m_config);
                recordDiagnosticsSummary(tool_name, *tool_it->second);
                return;
            }

            tool_it->second->executeBatch(files, m_config);
            recordDiagnosticsSummary(tool_name, *tool_it->second);
        }

        void runToolList(const std::vector<std::string>& tool_names, const std::string& file)
        {
            if (tool_names.empty())
            {
                return;
            }

            if (m_policy != std::launch::async || !m_threadPool || tool_names.size() == 1)
            {
                for (const auto& tool_name : tool_names)
                {
                    executeTool(tool_name, file);
                }
                return;
            }

            const auto containsExclusiveCaptureTool =
                std::any_of(tool_names.begin(), tool_names.end(), [](const std::string& tool_name)
                            { return requiresExclusiveProcessCapture(tool_name); });

            if (containsExclusiveCaptureTool)
            {
                std::vector<std::future<void>> parallelResults;
                parallelResults.reserve(tool_names.size());

                for (const auto& tool_name : tool_names)
                {
                    if (requiresExclusiveProcessCapture(tool_name))
                    {
                        continue;
                    }

                    parallelResults.push_back(m_threadPool->enqueue(
                        [this, tool_name, file] { executeTool(tool_name, file); }));
                }

                for (auto& result : parallelResults)
                {
                    result.get();
                }

                for (const auto& tool_name : tool_names)
                {
                    if (!requiresExclusiveProcessCapture(tool_name))
                    {
                        continue;
                    }
                    executeTool(tool_name, file);
                }
                return;
            }

            std::vector<std::future<void>> results;
            results.reserve(tool_names.size());

            for (const auto& tool_name : tool_names)
            {
                results.push_back(m_threadPool->enqueue([this, tool_name, file]
                                                        { executeTool(tool_name, file); }));
            }

            for (auto& result : results)
            {
                result.get();
            }
        }

        void runToolList(const std::vector<std::string>& tool_names,
                         const std::vector<std::string>& files)
        {
            if (tool_names.empty() || files.empty())
            {
                return;
            }

            std::vector<std::string> perFileTools;
            std::vector<std::string> batchTools;
            std::vector<std::string> unknownTools;

            perFileTools.reserve(tool_names.size());
            batchTools.reserve(tool_names.size());
            unknownTools.reserve(tool_names.size());

            for (const auto& tool_name : tool_names)
            {
                const auto tool_it = tools.find(tool_name);
                if (tool_it == tools.end())
                {
                    unknownTools.push_back(tool_name);
                    continue;
                }

                if (tool_it->second->supportsBatchExecution())
                {
                    batchTools.push_back(tool_name);
                }
                else
                {
                    perFileTools.push_back(tool_name);
                }
            }

            for (const auto& tool_name : unknownTools)
            {
                ctrace::Thread::Output::cerr("\033[31mUnknown tool: " + tool_name + "\033[0m");
            }

            for (const auto& file : files)
            {
                runToolList(perFileTools, file);
            }

            for (const auto& tool_name : batchTools)
            {
                executeBatchTool(tool_name, files);
            }
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

        static bool requiresExclusiveProcessCapture(const std::string& tool_name)
        {
            return tool_name == "ctrace_stack_analyzer";
        }

        void recordDiagnosticsSummary(const std::string& tool_name, const IAnalysisTool& tool)
        {
            const auto summary = tool.lastDiagnosticsSummary();
            coretrace::log(coretrace::Level::Info, coretrace::Module(tool_name),
                           "Diagnostics summary: info={}, warning={}, error={}\n", summary.info,
                           summary.warning, summary.error);

            std::lock_guard<std::mutex> lock(m_diagnosticsSummaryMutex);
            auto& total = m_diagnosticsSummaryByTool[tool_name];
            total.info += summary.info;
            total.warning += summary.warning;
            total.error += summary.error;
        }

        std::unordered_map<std::string, std::unique_ptr<IAnalysisTool>> tools;
        std::unordered_map<std::string, std::shared_ptr<std::mutex>> toolLocks;
        std::vector<std::string> static_tools;
        std::vector<std::string> dynamic_tools;
        ctrace::ProgramConfig m_config;
        std::size_t m_nbThreadPool;
        std::launch m_policy;
        std::shared_ptr<IpcStrategy> m_ipc;
        std::shared_ptr<ctrace::Thread::Output::CaptureBuffer> m_output_capture;
        std::unique_ptr<ThreadPool> m_threadPool;
        mutable std::mutex m_diagnosticsSummaryMutex;
        std::unordered_map<std::string, DiagnosticSummary> m_diagnosticsSummaryByTool;
    };
} // namespace ctrace

#endif // TOOLS_INVOKER_HPP
