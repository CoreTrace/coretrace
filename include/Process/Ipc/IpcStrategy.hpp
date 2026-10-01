// SPDX-License-Identifier: Apache-2.0
#ifndef IPC_STRATEGY_HPP
#define IPC_STRATEGY_HPP

#include <mutex>
#include <regex>

#include "../ProcessFactory.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

class IpcStrategy
{
  public:
    virtual ~IpcStrategy() = default;
    virtual void write(const std::string& data) = 0;
    virtual void close() = 0; // Pour cleanup RAII
};

namespace ctrace::ipc
{
    class SocketError : public std::runtime_error
    {
      public:
        explicit SocketError(const std::string& msg)
            : std::runtime_error("[IPC::SocketError] " + msg)
        {
        }
    };
} // namespace ctrace::ipc

class UnixSocketStrategy : public IpcStrategy
{
  private:
    int sock;
    std::string path;
    /// Tools running at the same time share one strategy: one message is sent at a time.
    std::mutex m_mutex;

#ifdef MSG_NOSIGNAL
    static constexpr int kSendFlags = MSG_NOSIGNAL;
#else
    static constexpr int kSendFlags = 0; // SO_NOSIGPIPE, set on the socket, does it instead.
#endif

  public:
    explicit UnixSocketStrategy(const std::string& socketPath) : sock(-1), path(socketPath)
    {
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock == -1)
        {
            throw std::runtime_error("Error socket creation: " + std::string(strerror(errno)));
        }
#ifdef SO_NOSIGPIPE
        // A reader that went away must be an error for write(), not a SIGPIPE ending ctrace.
        const int enabled = 1;
        (void)setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#endif
        sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == -1)
        {
            const std::string reason = strerror(errno);
            ::close(sock); // The destructor does not run when the constructor throws.
            sock = -1;
            throw std::runtime_error("Error socket connexion: " + reason);
        }
    }
    ~UnixSocketStrategy()
    {
        close();
    }
    /// Sends `data` whole: a stream socket may take a message in several sends.
    void write(const std::string& data) override
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        if (sock == -1)
        {
            throw ctrace::ipc::SocketError(std::string("Socket is not connected: ") +
                                           strerror(errno));
        }
        std::size_t sent = 0;
        while (sent < data.size())
        {
            const ssize_t count = send(sock, data.data() + sent, data.size() - sent, kSendFlags);
            if (count == -1)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                throw ctrace::ipc::SocketError(std::string("Connection failed: ") +
                                               strerror(errno));
            }
            sent += static_cast<std::size_t>(count);
        }
    }
    void close() override
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        if (sock != -1)
        {
            ::close(sock);
            sock = -1;
            // unlink(path.c_str());  // Optionnel
        }
    }
};

#endif // IPC_STRATEGY_HPP
