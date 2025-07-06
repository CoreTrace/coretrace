#ifndef PROCESSFACTORY_HPP
#define PROCESSFACTORY_HPP

#include <memory>
#include <iostream>
#include "Process.hpp"
#include "LinuxProcess.hpp"
#include "UnixProcess.hpp"
#include "WinProcess.hpp"

/**
 * @brief Factory class for creating platform-specific `Process` objects.
 *
 * The `ProcessFactory` class provides a static method to create instances of
 * platform-specific `Process` implementations based on the operating system.
 * This allows the application to abstract away platform-specific details when
 * managing processes.
 */
class ProcessFactory {
    public:
        /**
         * @brief Creates a platform-specific `Process` instance.
         *
         * This method determines the current operating system at compile time
         * and returns an instance of the appropriate `Process` subclass.
         *
         * @param command The command to execute.
         * @param args A vector of arguments to pass to the command (optional).
         * @return A `std::unique_ptr<Process>` pointing to the created process instance.
         *
         * @note The method uses preprocessor directives to select the appropriate
         *       implementation for Windows, Linux, or macOS.
         */
        static std::unique_ptr<Process> createProcess(const std::string& command, const std::vector<std::string>& args = {})
        {
            #if defined(_WIN32)
                ctrace::Thread::Output::cout("Creating Windows process for command: " + command);
                return std::make_unique<WindowsProcess>(command, args);
            #elif defined(__linux__)
                ctrace::Thread::Output::cout("Creating Linux process for command: " + command);
                return std::make_unique<UnixProcess>(command, args);
            #elif defined(__APPLE__)
                ctrace::Thread::Output::cout("Creating macOS process for command: " + command);
                // return std::make_unique<UnixProcessWithPosixSpawn>(command, args); // doesn't work with tscancode
                return std::make_unique<UnixProcess>(command, args);
            #endif
        }
};

#endif // PROCESSFACTORY_HPP
