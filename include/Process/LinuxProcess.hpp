#pragma once

#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstdio>
#include <stdexcept>
#include "Process.hpp"
#include "ThreadProcess.hpp"
#include <mutex>
#include <iostream>

class UnixProcess : public Process
{
  public:
    UnixProcess(const std::string& command, std::vector<std::string> args) : m_command(command)
    {
        m_arguments = args;
    }

  protected:
    void prepare() override
    {
        ctrace::Thread::Output::cout("Preparing Unix/Linux process");
        prepareArguments();
    }

    void run() override
    {
        ctrace::Thread::Output::cout("Running Unix/Linux process");

        char log_template[] = "/tmp/ctrace_process_log_XXXXXX";
        int logFd = mkstemp(log_template);
        if (logFd == -1)
        {
            throw std::runtime_error("Failed to create log file");
        }
        log_path_ = log_template;

        pid_t pid = fork();
        if (pid == -1)
        {
            close(logFd);
            throw std::runtime_error("Failed to fork process");
        }

        if (pid == 0)
        {
            std::vector<char*> execArgs;
            execArgs.push_back(const_cast<char*>(m_command.c_str()));
            for (auto& arg : m_arguments)
            {
                execArgs.push_back(const_cast<char*>(arg.c_str()));
            }
            execArgs.push_back(nullptr);

            dup2(logFd, STDOUT_FILENO);
            dup2(logFd, STDERR_FILENO);
            close(logFd);

            execvp(m_command.c_str(), execArgs.data());
            _exit(EXIT_FAILURE);
        }
        else
        {
            close(logFd);
            int status;
            waitpid(pid, &status, 0);
            captureLogs();
        }
    }

    void cleanup() override
    {
        ctrace::Thread::Output::cout("Cleaning up Unix/Linux process");
        if (!log_path_.empty())
        {
            unlink(log_path_.c_str());
            log_path_.clear();
        }
    }

    void prepareArguments() override
    {
        ctrace::Thread::Output::cout("Preparing arguments for Unix/Linux");
        // m_arguments.clear();
        // m_arguments.push_back("-l");
    }

    void captureLogs() override
    {
        ctrace::Thread::Output::cout("Capturing logs from process file");
        if (log_path_.empty())
        {
            return;
        }
        FILE* logFile = fopen(log_path_.c_str(), "r");
        if (!logFile)
        {
            throw std::runtime_error("Failed to open log file");
        }

        char buffer[1024];
        logOutput.clear();
        while (fgets(buffer, sizeof(buffer), logFile))
        {
            logOutput += buffer;
        }
        fclose(logFile);
        unlink(log_path_.c_str());
        log_path_.clear();

        // std::cout << "Logs captured: " << logOutput << "\n";
    }

  private:
    std::string m_command;
    std::string log_path_;
};
