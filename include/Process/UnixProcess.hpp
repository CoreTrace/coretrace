// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Process.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

/// Unix backend built on posix_spawn: safe from a multithreaded parent and reports spawn
/// failures as errors instead of a child that silently exits.
class UnixProcess : public Process
{
  public:
    UnixProcess(const std::string& command, std::vector<std::string> args)
        : command_(command), additionalArgs_(std::move(args))
    {
    }

    ~UnixProcess() override
    {
        closeLog();
    }

  protected:
    void prepare() override
    {
        resolveCommandPath();
        checkPermissions();
        m_arguments.clear();
        m_arguments.push_back(command_);
        m_arguments.insert(m_arguments.end(), additionalArgs_.begin(), additionalArgs_.end());
        openLog();
    }

    ProcessResult run() override
    {
        SpawnFileActions fileActions;
        if (posix_spawn_file_actions_adddup2(&fileActions.actions, logFd_, STDOUT_FILENO) != 0 ||
            posix_spawn_file_actions_adddup2(&fileActions.actions, logFd_, STDERR_FILENO) != 0)
        {
            throw std::runtime_error("Failed to redirect output of '" + command_ +
                                     "': " + std::string(std::strerror(errno)));
        }

        std::vector<char*> argv;
        argv.reserve(m_arguments.size() + 1);
        for (auto& arg : m_arguments)
        {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);

        pid_t pid = 0;
        const int spawnError = posix_spawn(&pid, resolvedPath_.c_str(), &fileActions.actions,
                                           nullptr, argv.data(), environ);
        if (spawnError != 0)
        {
            throw std::runtime_error("Failed to start '" + command_ +
                                     "': " + std::string(std::strerror(spawnError)));
        }

        int status = 0;
        while (waitpid(pid, &status, 0) == -1)
        {
            if (errno != EINTR)
            {
                throw std::runtime_error("Failed to wait for '" + command_ +
                                         "': " + std::string(std::strerror(errno)));
            }
        }

        ProcessResult result;
        if (WIFEXITED(status))
        {
            result.exitCode = WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status))
        {
            result.signal = WTERMSIG(status);
        }
        result.output = readLog();
        return result;
    }

    void cleanup() override
    {
        closeLog();
    }

  private:
    struct SpawnFileActions
    {
        posix_spawn_file_actions_t actions{};

        SpawnFileActions()
        {
            if (posix_spawn_file_actions_init(&actions) != 0)
            {
                throw std::runtime_error("Failed to init spawn file actions: " +
                                         std::string(std::strerror(errno)));
            }
        }

        ~SpawnFileActions()
        {
            posix_spawn_file_actions_destroy(&actions);
        }

        SpawnFileActions(const SpawnFileActions&) = delete;
        SpawnFileActions& operator=(const SpawnFileActions&) = delete;
    };

    void openLog()
    {
        char logTemplate[] = "/tmp/ctrace_process_log_XXXXXX";
        logFd_ = mkstemp(logTemplate);
        if (logFd_ == -1)
        {
            throw std::runtime_error("Failed to create temp log file: " +
                                     std::string(std::strerror(errno)));
        }
        unlink(logTemplate);
    }

    void closeLog()
    {
        if (logFd_ != -1)
        {
            close(logFd_);
            logFd_ = -1;
        }
    }

    std::string readLog()
    {
        std::string content;
        if (logFd_ == -1 || lseek(logFd_, 0, SEEK_SET) == -1)
        {
            return content;
        }
        char chunk[4096];
        ssize_t bytesRead = 0;
        while ((bytesRead = read(logFd_, chunk, sizeof(chunk))) > 0)
        {
            content.append(chunk, static_cast<std::size_t>(bytesRead));
        }
        return content;
    }

    void resolveCommandPath()
    {
        if (command_.find('/') != std::string::npos)
        {
            if (access(command_.c_str(), F_OK) == 0)
            {
                resolvedPath_ = command_;
                return;
            }
            throw std::runtime_error("Command not found: " + command_);
        }

        const char* pathEnv = std::getenv("PATH");
        if (pathEnv == nullptr)
        {
            throw std::runtime_error("Command not found in PATH (PATH is not set): " + command_);
        }

        std::string path(pathEnv);
        std::string::size_type start = 0;
        while (start <= path.size())
        {
            const auto end = path.find(':', start);
            const std::string dir =
                path.substr(start, end == std::string::npos ? std::string::npos : end - start);
            if (!dir.empty())
            {
                const std::string candidate = dir + "/" + command_;
                if (access(candidate.c_str(), X_OK) == 0)
                {
                    resolvedPath_ = candidate;
                    return;
                }
            }
            if (end == std::string::npos)
            {
                break;
            }
            start = end + 1;
        }

        throw std::runtime_error("Command not found in PATH: " + command_);
    }

    void checkPermissions()
    {
        if (access(resolvedPath_.c_str(), X_OK) != 0)
        {
            throw std::runtime_error("Command not executable: " + resolvedPath_ + ": " +
                                     std::string(std::strerror(errno)));
        }

        struct stat st{};
        if (stat(resolvedPath_.c_str(), &st) != 0)
        {
            throw std::runtime_error("Cannot stat command: " + resolvedPath_ + ": " +
                                     std::string(std::strerror(errno)));
        }
        if (!S_ISREG(st.st_mode))
        {
            throw std::runtime_error("Command is not a regular file: " + resolvedPath_);
        }
    }

    std::string command_;
    std::string resolvedPath_;
    std::vector<std::string> additionalArgs_;
    int logFd_ = -1;
};
