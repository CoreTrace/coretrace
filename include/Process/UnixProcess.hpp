#pragma once

#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdexcept>
#include <sys/stat.h>
#include "Process.hpp"

extern char** environ;

class UnixProcessWithPosixSpawn : public Process
{
  public:
    UnixProcessWithPosixSpawn(const std::string& command, std::vector<std::string> args)
        : command_(command), pid_(0), log_fd_(-1), resolved_path_("")
    {
        additional_args_ = args;
    }

    ~UnixProcessWithPosixSpawn() override
    {
        if (log_fd_ != -1)
        {
            close(log_fd_);
        }
    }

  protected:
    void prepare() override
    {
        resolveCommandPath();
        checkPermissions();
        prepareArguments();
        setupLogging();
    }

    void run() override
    {
        posix_spawn_file_actions_t file_actions;
        posix_spawnattr_t attr;

        if (posix_spawn_file_actions_init(&file_actions) != 0)
        {
            throw std::runtime_error("Failed to init file actions: " +
                                     std::string(strerror(errno)));
        }
        if (posix_spawnattr_init(&attr) != 0)
        {
            throw std::runtime_error("Failed to init spawn attr: " + std::string(strerror(errno)));
        }

        if (posix_spawn_file_actions_adddup2(&file_actions, log_fd_, STDOUT_FILENO) != 0 ||
            posix_spawn_file_actions_adddup2(&file_actions, log_fd_, STDERR_FILENO) != 0)
        {
            throw std::runtime_error("Failed to setup file redirection: " +
                                     std::string(strerror(errno)));
        }

        // Utiliser arguments (de la classe de base) au lieu de m_arguments
        std::vector<char*> argv;
        for (auto& arg : m_arguments)
        {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        int status =
            posix_spawn(&pid_, resolved_path_.c_str(), &file_actions, &attr, argv.data(), environ);

        if (status != 0)
        {
            throw std::runtime_error("posix_spawn failed: " + std::string(strerror(status)));
        }

        posix_spawn_file_actions_destroy(&file_actions);
        posix_spawnattr_destroy(&attr);
    }

    void cleanup() override
    {
        if (pid_ > 0)
        {
            int status;
            waitpid(pid_, &status, 0);
            pid_ = 0;
        }
        captureLogs();
        if (log_fd_ != -1)
        {
            close(log_fd_);
            log_fd_ = -1;
        }
    }

    void prepareArguments() override
    {
        m_arguments.clear();

        // Le premier argument doit être le nom de la commande
        std::string cmd_name = resolved_path_.substr(resolved_path_.find_last_of("/\\") + 1);
        m_arguments.push_back(cmd_name);

        // Ajouter les arguments supplémentaires
        m_arguments.insert(m_arguments.end(), additional_args_.begin(), additional_args_.end());
    }

    void captureLogs() override
    {
        if (log_fd_ == -1)
        {
            return;
        }

        lseek(log_fd_, 0, SEEK_SET);

        std::string buffer;
        char chunk[1024];
        ssize_t bytes_read;

        while ((bytes_read = read(log_fd_, chunk, sizeof(chunk) - 1)) > 0)
        {
            chunk[bytes_read] = '\0';
            buffer += chunk;
        }

        logOutput = buffer;
    }

  private:
    void setupLogging()
    {
        char temp_path[] = "/tmp/process_log_XXXXXX";
        log_fd_ = mkstemp(temp_path);
        if (log_fd_ == -1)
        {
            throw std::runtime_error("Failed to create temp log file: " +
                                     std::string(strerror(errno)));
        }
        unlink(temp_path);
    }

    void resolveCommandPath()
    {
        if (command_.find('/') != std::string::npos)
        {
            if (access(command_.c_str(), F_OK) == 0)
            {
                resolved_path_ = command_;
                return;
            }
            throw std::runtime_error("Command not found: " + command_);
        }

        char* path_env = getenv("PATH");
        if (!path_env)
        {
            throw std::runtime_error("PATH environment variable not set");
        }

        std::string path(path_env);
        std::string::size_type start = 0;
        std::string::size_type end;

        while ((end = path.find(':', start)) != std::string::npos)
        {
            std::string dir = path.substr(start, end - start);
            std::string full_path = dir + "/" + command_;

            if (access(full_path.c_str(), F_OK) == 0)
            {
                resolved_path_ = full_path;
                return;
            }
            start = end + 1;
        }

        std::string last_dir = path.substr(start);
        std::string last_path = last_dir + "/" + command_;
        if (access(last_path.c_str(), F_OK) == 0)
        {
            resolved_path_ = last_path;
            return;
        }

        throw std::runtime_error("Command not found in PATH: " + command_);
    }

    void checkPermissions()
    {
        if (access(resolved_path_.c_str(), X_OK) != 0)
        {
            throw std::runtime_error("Command not executable: " + resolved_path_ + ": " +
                                     std::string(strerror(errno)));
        }

        struct stat st;
        if (stat(resolved_path_.c_str(), &st) != 0)
        {
            throw std::runtime_error("Cannot stat command: " + resolved_path_ + ": " +
                                     std::string(strerror(errno)));
        }

        if (!S_ISREG(st.st_mode))
        {
            throw std::runtime_error("Command is not a regular file: " + resolved_path_);
        }
    }

    std::string command_;
    std::string resolved_path_;
    pid_t pid_;
    int log_fd_;
    std::vector<std::string> additional_args_; // Remplacer m_arguments par additional_args_
};
