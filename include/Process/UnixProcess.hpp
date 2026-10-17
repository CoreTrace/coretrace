// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Interrupt.hpp"
#include "Process.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
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

        // The child leads its own process group, so that stopping it (time limit, interrupt) also
        // stops whatever it started, and nothing it started outlives it. The terminal's Ctrl-C
        // reaches only ctrace, which stops it.
        SpawnAttributes attributes;
        if (posix_spawnattr_setflags(&attributes.attributes, POSIX_SPAWN_SETPGROUP) != 0 ||
            posix_spawnattr_setpgroup(&attributes.attributes, 0) != 0)
        {
            throw std::runtime_error("Failed to set the process group of '" + command_ + "'");
        }

        if (ctrace::interrupt::toolsMustStop())
        {
            throw std::runtime_error("ctrace was interrupted: '" + command_ + "' was not started");
        }
        pid_t pid = 0;
        const int spawnError = posix_spawn(&pid, resolvedPath_.c_str(), &fileActions.actions,
                                           &attributes.attributes, argv.data(), environ);
        if (spawnError != 0)
        {
            throw std::runtime_error("Failed to start '" + command_ +
                                     "': " + std::string(std::strerror(spawnError)));
        }

        ProcessResult result;
        const int status = waitForChild(pid, result);
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
    // ponytail: polls every 20 ms; a SIGCHLD-driven wait if a deadline or an interrupt ever
    // needs to be acted on faster than that.
    static constexpr auto kPollInterval = std::chrono::milliseconds(20);
    /// How long an interrupted tool has to end on SIGTERM before it gets SIGKILL.
    static constexpr auto kStopGracePeriod = std::chrono::seconds(2);

    /// Waits for `pid`, stopping its process group once the time limit has passed, once ctrace
    /// is interrupted, or once the tool has ended.
    [[nodiscard]] int waitForChild(pid_t pid, ProcessResult& result)
    {
        const auto deadline = std::chrono::steady_clock::now() + m_timeout;
        int status = 0;
        for (;;)
        {
            if (ended(pid))
            {
                // What the tool left in its group would outlive the run, unwatched: an interrupt
                // handled on another thread just after this point could no longer reach it. It
                // ends with the tool, which is reaped last, so that its group id cannot have been
                // given to another process.
                (void)kill(-pid, SIGKILL);
                (void)waitpid(pid, &status, 0);
                return status;
            }
            if (ctrace::interrupt::toolsMustStop())
            {
                result.interrupted = true;
                return stop(pid);
            }
            if (m_timeout.count() > 0 && std::chrono::steady_clock::now() >= deadline)
            {
                result.timedOut = true;
                (void)kill(-pid, SIGKILL);
                (void)waitpid(pid, &status, 0);
                return status;
            }
            std::this_thread::sleep_for(kPollInterval);
        }
    }

    /// Stops the process group of `pid`: SIGTERM, then SIGKILL once the tool has ended or the
    /// grace period has passed, for what it started may outlive it. The tool is reaped last: until
    /// then, its group id cannot be given to another process.
    [[nodiscard]] int stop(pid_t pid) const
    {
        (void)kill(-pid, SIGTERM);
        const auto grace = std::chrono::steady_clock::now() + kStopGracePeriod;
        while (!ended(pid) && std::chrono::steady_clock::now() < grace)
        {
            std::this_thread::sleep_for(kPollInterval);
        }
        (void)kill(-pid, SIGKILL);
        int status = 0;
        (void)waitpid(pid, &status, 0);
        return status;
    }

    /// Whether `pid` has ended, leaving it to be reaped.
    [[nodiscard]] bool ended(pid_t pid) const
    {
        siginfo_t info{};
        if (waitid(P_PID, static_cast<id_t>(pid), &info, WEXITED | WNOHANG | WNOWAIT) == -1)
        {
            if (errno == EINTR)
            {
                return false;
            }
            throw std::runtime_error("Failed to wait for '" + command_ +
                                     "': " + std::string(std::strerror(errno)));
        }
        return info.si_pid == pid;
    }

    struct SpawnAttributes
    {
        posix_spawnattr_t attributes{};

        SpawnAttributes()
        {
            if (posix_spawnattr_init(&attributes) != 0)
            {
                throw std::runtime_error("Failed to init spawn attributes: " +
                                         std::string(std::strerror(errno)));
            }
        }

        ~SpawnAttributes()
        {
            posix_spawnattr_destroy(&attributes);
        }

        SpawnAttributes(const SpawnAttributes&) = delete;
        SpawnAttributes& operator=(const SpawnAttributes&) = delete;
    };

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

    /// What a PATH lookup accepts, as a shell does: an executable regular file. A directory is
    /// searchable, so execute permission alone would let it hide a later PATH entry.
    [[nodiscard]] static bool isExecutableFile(const std::string& path)
    {
        struct stat st{};
        return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) &&
               access(path.c_str(), X_OK) == 0;
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
                if (isExecutableFile(candidate))
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
