// SPDX-License-Identifier: Apache-2.0
//
// The deprecated Unix socket transport: what a reader receives, and a failure reported as an
// error instead of ending the process.
//
// Without arguments, UnixSocketStrategy is checked on its own. With
// `--ctrace <ctrace> <C source>`, the same contract is checked through the command line.
#include "Process/Ipc/IpcStrategy.hpp"
#include "Process/ProcessFactory.hpp"

#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace
{
    struct TestReport
    {
        int failures = 0;

        void expect(bool condition, const std::string& message)
        {
            if (condition)
            {
                std::cout << "[PASS] " << message << "\n";
                return;
            }
            ++failures;
            std::cerr << "[FAIL] " << message << "\n";
        }
    };

    constexpr std::chrono::seconds kPeerTimeout{60};

    /// A listening Unix socket for the duration of the scope. The path stays short: a
    /// sockaddr_un holds about a hundred bytes, less than some temporary directories need.
    class Listener
    {
      public:
        Listener() : path_("/tmp/ctrace-ipc-test-" + std::to_string(::getpid()) + ".sock")
        {
            ::unlink(path_.c_str());
            fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
            sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            std::strncpy(addr.sun_path, path_.c_str(), sizeof(addr.sun_path) - 1);
            if (fd_ == -1 || ::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
                ::listen(fd_, 1) != 0)
            {
                throw std::runtime_error("cannot listen on " + path_ + ": " + std::strerror(errno));
            }
        }

        ~Listener()
        {
            ::close(fd_);
            ::unlink(path_.c_str());
        }

        Listener(const Listener&) = delete;
        Listener& operator=(const Listener&) = delete;

        [[nodiscard]] const std::string& path() const
        {
            return path_;
        }

        /// The next connection, or -1 when none arrives in time.
        [[nodiscard]] int accept() const
        {
            pollfd ready{fd_, POLLIN, 0};
            const auto waitMs = std::chrono::milliseconds(kPeerTimeout).count();
            if (::poll(&ready, 1, static_cast<int>(waitMs)) != 1)
            {
                return -1;
            }
            return ::accept(fd_, nullptr, nullptr);
        }

      private:
        std::string path_;
        int fd_ = -1;
    };

    /// Everything the peer sends until it closes; what arrived so far when it stays silent.
    std::string readUntilClosed(int fd)
    {
        std::string received;
        char chunk[4096];
        pollfd ready{fd, POLLIN, 0};
        const auto waitMs = std::chrono::milliseconds(kPeerTimeout).count();
        while (::poll(&ready, 1, static_cast<int>(waitMs)) == 1)
        {
            const ssize_t count = ::read(fd, chunk, sizeof(chunk));
            if (count <= 0)
            {
                break;
            }
            received.append(chunk, static_cast<std::size_t>(count));
        }
        return received;
    }

    /// The descriptor the next open() gets: the lowest one free.
    int lowestFreeDescriptor()
    {
        const int fd = ::open("/dev/null", O_RDONLY);
        ::close(fd);
        return fd;
    }

    void testWritesReachTheReader(TestReport& report)
    {
        const Listener listener;
        {
            UnixSocketStrategy socket(listener.path());
            socket.write("first ");
            socket.write("second");
            socket.close();
        }
        const int peer = listener.accept();
        report.expect(peer != -1, "the strategy connects to the socket path");
        report.expect(readUntilClosed(peer) == "first second",
                      "every write reaches the reader, in order, then close() ends the stream");
        ::close(peer);
    }

    void testDestructorClosesTheStream(TestReport& report)
    {
        const Listener listener;
        {
            UnixSocketStrategy socket(listener.path());
            socket.write("last words");
        }
        const int peer = listener.accept();
        report.expect(readUntilClosed(peer) == "last words",
                      "the destructor closes the stream: the reader sees the end of it");
        ::close(peer);
    }

    void testFailedConnectLeaksNoDescriptor(TestReport& report)
    {
        const int before = lowestFreeDescriptor();
        std::string message;
        try
        {
            UnixSocketStrategy socket("/nonexistent/ctrace-ipc-test.sock");
        }
        catch (const std::runtime_error& error)
        {
            message = error.what();
        }
        report.expect(!message.empty(), "an unreachable socket path is an error (" + message + ")");
        report.expect(lowestFreeDescriptor() == before,
                      "a failed connection closes the socket it created");
    }

    void testClosedReaderIsAnError(TestReport& report)
    {
        const Listener listener;
        UnixSocketStrategy socket(listener.path());
        ::close(listener.accept());

        std::string message;
        try
        {
            socket.write("nobody is listening");
        }
        catch (const ctrace::ipc::SocketError& error)
        {
            message = error.what();
        }
        report.expect(!message.empty(),
                      "writing to a reader that closed is an error, not a SIGPIPE (" + message +
                          ")");
    }

    ProcessResult runCtrace(const std::string& ctrace, const std::string& socketPath,
                            const std::string& source)
    {
        auto process = ProcessFactory::createProcess(
            ctrace, {"--ipc", "socket", "--ipc-path", socketPath, "--invoke",
                     "ctrace_stack_analyzer", "--fail-on", "none", "--input", source});
        return process->execute();
    }

    void testCliDeliversTheReport(TestReport& report, const std::string& ctrace,
                                  const std::string& source)
    {
        const Listener listener;
        std::string received;
        std::thread reader(
            [&]
            {
                const int peer = listener.accept();
                received = readUntilClosed(peer);
                ::close(peer);
            });
        const ProcessResult run = runCtrace(ctrace, listener.path(), source);
        reader.join();

        report.expect(run.signal == 0 && run.exitCode == 0,
                      "ctrace --ipc socket completes (exit " + std::to_string(run.exitCode) +
                          ", signal " + std::to_string(run.signal) + ")");
        report.expect(received.find("double release") != std::string::npos,
                      "the stack analyzer's report reaches the socket (received " +
                          std::to_string(received.size()) + " bytes)");
    }

    void testCliSurvivesAReaderThatCloses(TestReport& report, const std::string& ctrace,
                                          const std::string& source)
    {
        const Listener listener;
        std::thread reader([&] { ::close(listener.accept()); });
        const ProcessResult run = runCtrace(ctrace, listener.path(), source);
        reader.join();

        report.expect(run.signal == 0, "a reader that closes does not kill ctrace (signal " +
                                           std::to_string(run.signal) + ")");
        report.expect(run.exitCode == 3, "the undelivered report makes the analysis incomplete "
                                         "(exit " +
                                             std::to_string(run.exitCode) + ")");
        report.expect(run.output.find("SocketError") != std::string::npos,
                      "the socket error is reported");
    }
} // namespace

int main(int argc, char* argv[])
{
    TestReport report;
    if (argc == 4 && std::string(argv[1]) == "--ctrace")
    {
        testCliDeliversTheReport(report, argv[2], argv[3]);
        testCliSurvivesAReaderThatCloses(report, argv[2], argv[3]);
    }
    else if (argc == 1)
    {
        testWritesReachTheReader(report);
        testDestructorClosesTheStream(report);
        testFailedConnectLeaksNoDescriptor(report);
        testClosedReaderIsAnError(report);
    }
    else
    {
        std::cerr << "Usage: ctrace_ipc_socket_tests [--ctrace <ctrace> <C source>]\n";
        return 2;
    }

    if (report.failures == 0)
    {
        std::cout << "ipc_socket_tests: all checks passed\n";
        return 0;
    }
    std::cerr << report.failures << " IPC socket check(s) failed\n";
    return 1;
}
