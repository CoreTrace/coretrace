// SPDX-License-Identifier: Apache-2.0
#ifndef IPC_STRATEGY_HPP
#define IPC_STRATEGY_HPP

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
    void write(const std::string& data) override
    {
        if (sock == -1)
        {
            throw ctrace::ipc::SocketError(std::string("Socket is not connected: ") +
                                           strerror(errno));
        }
        if (send(sock, data.c_str(), data.size(), kSendFlags) == -1)
        {
            throw ctrace::ipc::SocketError(std::string("Connection failed: ") + strerror(errno));
        }
    }
    void close() override
    {
        if (sock != -1)
        {
            ::close(sock);
            sock = -1;
            // unlink(path.c_str());  // Optionnel
        }
    }
};

#endif // IPC_STRATEGY_HPP
