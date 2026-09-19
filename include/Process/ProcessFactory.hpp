// SPDX-License-Identifier: Apache-2.0
#ifndef PROCESSFACTORY_HPP
#define PROCESSFACTORY_HPP

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
    static std::unique_ptr<Process> createProcess(const std::string& command,
                                                  const std::vector<std::string>& args = {})
    {
#if defined(_WIN32)
        coretrace::log(coretrace::Level::Debug, "Creating Windows process for command: {}\n",
                       command);
        return std::make_unique<WindowsProcess>(command, args);
#else
        coretrace::log(coretrace::Level::Debug, "Creating Unix process for command: {}\n", command);
        return std::make_unique<UnixProcess>(command, args);
#endif
    }
};

#endif // PROCESSFACTORY_HPP
