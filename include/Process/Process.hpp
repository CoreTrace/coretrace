// SPDX-License-Identifier: Apache-2.0
#ifndef PROCESS_HPP
#define PROCESS_HPP

#include <string>
#include <vector>
#include <memory>
#include <sstream>

class Process
{
  public:
    virtual ~Process() = default;

    void execute(void)
    {
        prepare();
        run();
        cleanup();
    }

    std::string logOutput;

    /// The child's exit status, once it has been waited for. A tool that
    /// started and then failed says so here; without it a failing tool is
    /// indistinguishable from one that found nothing.
    [[nodiscard]] int exitCode() const
    {
        return m_exitCode;
    }

  protected:
    int m_exitCode = -1;

    virtual void prepare() = 0;
    virtual void run() = 0;
    virtual void cleanup() = 0;

    virtual void prepareArguments() = 0;
    virtual void captureLogs() = 0;

    std::vector<std::string> m_arguments;
    std::stringstream log_buffer;
};

#endif // PROCESS_HPP
