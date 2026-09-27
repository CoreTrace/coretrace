// SPDX-License-Identifier: Apache-2.0
#ifndef PROCESSFACTORY_HPP
#define PROCESSFACTORY_HPP

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "Process.hpp"

#include <coretrace/logger.hpp>
#if defined(_WIN32)
#include "WinProcess.hpp"
#else
#include "UnixProcess.hpp"
#endif

/// Creates the platform-specific `Process` for one command.
class ProcessFactory
{
  public:
    /// `timeout`: see Process::setTimeout; zero waits for the child however long it runs.
    static std::unique_ptr<Process> createProcess(const std::string& command,
                                                  const std::vector<std::string>& args = {},
                                                  std::chrono::milliseconds timeout = {})
    {
#if defined(_WIN32)
        coretrace::log(coretrace::Level::Debug, "Creating Windows process for command: {}\n",
                       command);
        std::unique_ptr<Process> process = std::make_unique<WindowsProcess>(command, args);
#else
        coretrace::log(coretrace::Level::Debug, "Creating Unix process for command: {}\n", command);
        std::unique_ptr<Process> process = std::make_unique<UnixProcess>(command, args);
#endif
        process->setTimeout(timeout);
        return process;
    }
};

#endif // PROCESSFACTORY_HPP
