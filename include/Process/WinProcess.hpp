// SPDX-License-Identifier: Apache-2.0
#pragma once

#ifdef _WIN32

#include "Process.hpp"

#define NOMINMAX
#include <windows.h>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class WindowsProcess : public Process
{
  public:
    WindowsProcess(std::string command, std::vector<std::string> args)
        : m_command(std::move(command)), m_arguments_extra(std::move(args))
    {
    }

  protected:
    void prepare() override
    {
        prepareArguments();
    }

    void run() override
    {
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE stdoutRead = nullptr;
        HANDLE stdoutWrite = nullptr;

        if (!CreatePipe(&stdoutRead, &stdoutWrite, &sa, 0))
        {
            throw std::runtime_error("Failed to create stdout pipe");
        }
        if (!SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0))
        {
            CloseHandle(stdoutRead);
            CloseHandle(stdoutWrite);
            throw std::runtime_error("Failed to configure stdout pipe");
        }

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = stdoutWrite;
        si.hStdError = stdoutWrite;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

        PROCESS_INFORMATION pi{};
        std::string commandLine = buildCommandLine();
        std::vector<char> mutableCommandLine(commandLine.begin(), commandLine.end());
        mutableCommandLine.push_back('\0');

        const BOOL created =
            CreateProcessA(nullptr, mutableCommandLine.data(), nullptr, nullptr, TRUE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

        CloseHandle(stdoutWrite);
        if (!created)
        {
            CloseHandle(stdoutRead);
            throw std::runtime_error("Failed to create process: " + lastErrorMessage());
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        m_stdoutRead = stdoutRead;
        captureLogs();
    }

    void cleanup() override
    {
        if (m_stdoutRead != nullptr)
        {
            CloseHandle(m_stdoutRead);
            m_stdoutRead = nullptr;
        }
    }

    void prepareArguments() override
    {
        m_arguments = m_arguments_extra;
    }

    void captureLogs() override
    {
        if (m_stdoutRead == nullptr)
        {
            return;
        }

        logOutput.clear();
        char buffer[4096];
        DWORD bytesRead = 0;
        while (ReadFile(m_stdoutRead, buffer, sizeof(buffer), &bytesRead, nullptr) &&
               bytesRead > 0)
        {
            logOutput.append(buffer, buffer + bytesRead);
        }
    }

  private:
    [[nodiscard]] static std::string quoteArgument(const std::string& argument)
    {
        if (argument.empty())
        {
            return "\"\"";
        }

        bool needsQuotes = false;
        for (const char ch : argument)
        {
            if (ch == ' ' || ch == '\t' || ch == '"')
            {
                needsQuotes = true;
                break;
            }
        }
        if (!needsQuotes)
        {
            return argument;
        }

        std::string quoted = "\"";
        std::size_t backslashCount = 0;
        for (const char ch : argument)
        {
            if (ch == '\\')
            {
                ++backslashCount;
                continue;
            }
            if (ch == '"')
            {
                quoted.append(backslashCount * 2 + 1, '\\');
                quoted.push_back('"');
                backslashCount = 0;
                continue;
            }

            if (backslashCount > 0)
            {
                quoted.append(backslashCount, '\\');
                backslashCount = 0;
            }
            quoted.push_back(ch);
        }

        if (backslashCount > 0)
        {
            quoted.append(backslashCount * 2, '\\');
        }
        quoted.push_back('"');
        return quoted;
    }

    [[nodiscard]] static std::string lastErrorMessage()
    {
        const DWORD error = GetLastError();
        LPSTR messageBuffer = nullptr;
        const DWORD size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&messageBuffer), 0, nullptr);

        std::string message =
            (size > 0 && messageBuffer != nullptr) ? std::string(messageBuffer, size)
                                                   : ("Windows error " + std::to_string(error));
        if (messageBuffer != nullptr)
        {
            LocalFree(messageBuffer);
        }
        while (!message.empty() &&
               (message.back() == '\r' || message.back() == '\n' || message.back() == ' '))
        {
            message.pop_back();
        }
        return message;
    }

    [[nodiscard]] std::string buildCommandLine() const
    {
        std::string commandLine = quoteArgument(m_command);
        for (const auto& argument : m_arguments)
        {
            commandLine.push_back(' ');
            commandLine += quoteArgument(argument);
        }
        return commandLine;
    }

    std::string m_command;
    std::vector<std::string> m_arguments_extra;
    HANDLE m_stdoutRead = nullptr;
};

#endif
