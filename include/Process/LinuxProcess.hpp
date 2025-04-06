#pragma once

#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdexcept>
#include "Process.hpp"

class UnixProcess : public Process {
public:
    UnixProcess(const std::string& command, std::vector<std::string> args) : m_command(command)
    {
        m_arguments = args;
    }

protected:
    void prepare() override
    {
        std::cout << "Preparing Unix/Linux process\n";
        prepareArguments();
    }

    void run() override {
        std::cout << "Running Unix/Linux process\n";

        pid_t pid = fork();
        if (pid == -1) {
            throw std::runtime_error("Failed to fork process");
        }

        if (pid == 0) { // Processus fils
            // Convertir les arguments en tableau de char* pour execvp
            std::vector<char*> execArgs;
            execArgs.push_back(const_cast<char*>(m_command.c_str()));
            for (auto& arg : m_arguments) {
                execArgs.push_back(const_cast<char*>(arg.c_str()));
            }
            execArgs.push_back(nullptr);

            // Rediriger la sortie standard vers un fichier temporaire pour capturer les logs
            int logFd = open("process.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (logFd == -1) {
                throw std::runtime_error("Failed to open log file");
            }
            dup2(logFd, STDOUT_FILENO);
            dup2(logFd, STDERR_FILENO);
            close(logFd);

            execvp(m_command.c_str(), execArgs.data());
            _exit(EXIT_FAILURE); // En cas d'échec d'execvp
        } else { // Processus parent
            int status;
            waitpid(pid, &status, 0);
            captureLogs();
        }
    }

    void cleanup() override {
        std::cout << "Cleaning up Unix/Linux process\n";
        unlink("process.log"); // Supprimer le fichier de logs temporaire
    }

    void prepareArguments() override {
        std::cout << "Preparing arguments for Unix/Linux\n";
        // m_arguments.clear();
        // m_arguments.push_back("-l"); // Exemple d'argument pour `ls`
    }

    void captureLogs() override {
        std::cout << "Capturing logs for Unix/Linux\n";
        FILE* logFile = fopen("process.log", "r");
        if (!logFile) {
            throw std::runtime_error("Failed to open log file");
        }

        char buffer[1024];
        logOutput.clear();
        while (fgets(buffer, sizeof(buffer), logFile)) {
            logOutput += buffer;
        }
        fclose(logFile);

        // std::cout << "Logs captured: " << logOutput << "\n";
    }

private:
    std::string m_command;
};
