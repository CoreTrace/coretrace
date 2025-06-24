#ifndef PROCESS_HPP
#define PROCESS_HPP

#include <string>
#include <vector>
#include <memory>

class Process {
    public:
        virtual ~Process() = default;

        void execute(void)
        {
            prepare();
            run();
            cleanup();
        }

        std::string logOutput;
    protected:
        virtual void prepare() = 0;
        virtual void run() = 0;
        virtual void cleanup() = 0;

        virtual void prepareArguments() = 0;
        virtual void captureLogs() = 0;

        std::vector<std::string> m_arguments;
        std::stringstream log_buffer;
};

#endif // PROCESS_HPP
