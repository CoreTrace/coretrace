#ifndef TOOLS_INVOKER_HPP
#define TOOLS_INVOKER_HPP

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <iostream>

#include "AnalysisTools.hpp"

#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <future>
#include <chrono>
#include <functional>
#include <sstream>

std::mutex queueMutex;

class ThreadPool {
public:
    ThreadPool(size_t numThreads)
    {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queueMutex);
                        condition.wait(lock, [this] { return !tasks.empty() || stop; });
                        if (stop && tasks.empty()) return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (auto& worker : workers) {
            worker.join();
        }
    }

    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using return_type = decltype(f(args...));
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            if (stop) throw std::runtime_error("Enqueue on stopped ThreadPool");
            tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return res;
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop = false;
};


namespace ctrace
{

class ToolInvoker {
    public:
        ToolInvoker(ctrace::ProgramConfig config, uint8_t nbThreadPool, std::launch policy)
            : m_config(config), m_nbThreadPool(nbThreadPool), m_policy(policy)
        {
            std::cout << "\033[36mInitializing ToolInvoker...\033[0m\n";

            tools["cppcheck"]   = std::make_unique<CppCheckToolImplementation>();
            tools["flawfinder"] = std::make_unique<FlawfinderToolImplementation>();
            tools["tscancode"]  = std::make_unique<TscancodeToolImplementation>();
            tools["ikos"]       = std::make_unique<IkosToolImplementation>();
            tools["dyn_tools_1"] = std::make_unique<DynTool1>();
            tools["dyn_tools_2"] = std::make_unique<DynTool2>();
            tools["dyn_tools_3"] = std::make_unique<DynTool3>();

            static_tools = {"cppcheck", "flawfinder", "tscancode", "ikos"};
            dynamic_tools = {"dyn_tools_1", "dyn_tools_2", "dyn_tools_3"};

            if (m_config.global.ipc == "standardIO")
            {
                m_ipc = nullptr; // Use std::cout directely
                std::cout << "\033[36mUsing standardIO for IPC.\033[0m\n";
            }
            else
            {
                m_ipc = std::make_shared<UnixSocketStrategy>(m_config.global.ipcPath);

                for (auto& [_, tool] : tools)
                    tool->setIpcStrategy(m_ipc);
            }
        }

        // execute all static analysis tools
        void runStaticTools(const std::string& file) const
        {
            std::vector<std::future<void>> results;
            ThreadPool pool(m_nbThreadPool);

            for (const auto& tool_name : static_tools) {
                // tools.at(tool_name)->execute(file, m_config);
                results.push_back(std::async(
                    m_policy,
                    [&tool = tools.at(tool_name), file, this]()
                    {
                        return tool->execute(file, m_config);
                    }
                ));
            }
            for (auto& result : results)
            {
                result.get();
            }
        }

        // execute all dynamic analysis tools
        void runDynamicTools(const std::string& file) const
        {
            for (const auto& tool_name : dynamic_tools)
            {
                tools.at(tool_name)->execute(file, m_config);
            }
        }

        // Exécute une liste spécifique d'outils
        void runSpecificTools(const std::vector<std::string>& tool_names, const std::string& file) const
        {
            for (const auto& name : tool_names)
            {
                if (tools.count(name))
                {
                    tools.at(name)->execute(file, m_config);
                }
                else
                {
                    ctrace::Thread::Output::cerr("\033[31mUnknown tool: " + name + "\033[0m");
                }
            }
        }

    private:
        std::unordered_map<std::string, std::unique_ptr<IAnalysisTool>> tools;
        std::vector<std::string> static_tools;
        std::vector<std::string> dynamic_tools;
        ctrace::ProgramConfig m_config;
        uint8_t m_nbThreadPool;
        std::launch m_policy;
        std::shared_ptr<IpcStrategy> m_ipc;
    };
}

#endif // TOOLS_INVOKER_HPP
