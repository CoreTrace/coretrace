#ifndef THREAD_PROCESS_HPP
#define THREAD_PROCESS_HPP

namespace ctrace
{
    namespace Thread
    {
        namespace Output
        {
            static std::mutex cout_mutex;

            static void cout(const std::string& message) {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << message << std::endl;
            }
            static void cerr(const std::string& message) {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cerr << message << std::endl;
            }
        }
    }
}

#endif // THREAD_PROCESS_HPP
