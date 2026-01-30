#pragma once

#ifdef _WIN32

#include <iostream>
#include <windows.h>
#include <stdexcept>

class WindowsProcess : public Process
{
  public:
    WindowsProcess(const std::string& command) : command(command) {}

  protected:
    void prepare() override
    {
        std::cout << "Preparing Windows process\n";
        prepareArguments();
    }

    void run() override
    {
        std::cout << "Running Windows process\n";

        // Préparer la commande avec les arguments
        std::string cmdLine = command;
        for (const auto& arg : arguments)
        {
            cmdLine += " " + arg;
        }

        STARTUPINFO si = {sizeof(si)};
        PROCESS_INFORMATION pi;
        SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};

        // Créer un fichier pour capturer les logs
        HANDLE logFile = CreateFile("process.log", GENERIC_WRITE, FILE_SHARE_READ, &sa,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (logFile == INVALID_HANDLE_VALUE)
        {
            throw std::runtime_error("Failed to create log file");
        }

        si.hStdOutput = logFile;
        si.hStdError = logFile;
        si.dwFlags |= STARTF_USESTDHANDLES;

        // Lancer le processus
        if (!CreateProcess(NULL, const_cast<char*>(cmdLine.c_str()), NULL, NULL, TRUE, 0, NULL,
                           NULL, &si, &pi))
        {
            CloseHandle(logFile);
            throw std::runtime_error("Failed to create process");
        }

        // Attendre que le processus se termine
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(logFile);

        captureLogs();
    }

    void cleanup() override
    {
        std::cout << "Cleaning up Windows process\n";
        DeleteFile("process.log"); // Supprimer le fichier de logs temporaire
    }

    void prepareArguments() override
    {
        std::cout << "Preparing arguments for Windows\n";
        arguments.clear();
        arguments.push_back("/C");  // Exemple pour `cmd.exe`
        arguments.push_back("dir"); // Commande `dir` pour lister les fichiers
    }

    void captureLogs() override
    {
        std::cout << "Capturing logs for Windows\n";
        FILE* logFile = fopen("process.log", "r");
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

        std::cout << "Logs captured: " << logOutput << "\n";
    }

  private:
    std::string command;
};

#endif
