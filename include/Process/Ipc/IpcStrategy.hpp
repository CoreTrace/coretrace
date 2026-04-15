// SPDX-License-Identifier: Apache-2.0
#ifndef IPC_STRATEGY_HPP
#define IPC_STRATEGY_HPP

#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

class IpcStrategy
{
  public:
    virtual ~IpcStrategy() = default;
    virtual void write(const std::string& data) = 0;
    virtual void close() = 0;
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

#ifdef _WIN32
    class WinsockSession
    {
      public:
        WinsockSession()
        {
            WSADATA wsaData{};
            const int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (result != 0)
            {
                throw SocketError("WSAStartup failed: " + std::to_string(result));
            }
        }

        ~WinsockSession()
        {
            WSACleanup();
        }
    };

    [[nodiscard]] inline const WinsockSession& winsockSession()
    {
        static const WinsockSession session;
        return session;
    }

    [[nodiscard]] inline std::string lastSocketError()
    {
        return "Winsock error " + std::to_string(WSAGetLastError());
    }
#else
    [[nodiscard]] inline std::string lastSocketError()
    {
        return std::strerror(errno);
    }
#endif
} // namespace ctrace::ipc

class LocalSocketStrategy : public IpcStrategy
{
  public:
    explicit LocalSocketStrategy(const std::string& socketPath) : path_(socketPath)
    {
#ifdef _WIN32
        (void)ctrace::ipc::winsockSession();
        socket_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (socket_ == INVALID_SOCKET)
        {
            throw ctrace::ipc::SocketError("Error creating AF_UNIX socket: " +
                                           ctrace::ipc::lastSocketError());
        }
#else
        socket_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (socket_ == -1)
        {
            throw ctrace::ipc::SocketError("Error creating AF_UNIX socket: " +
                                           ctrace::ipc::lastSocketError());
        }
#endif

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        if (path_.size() >= sizeof(addr.sun_path))
        {
            close();
            throw ctrace::ipc::SocketError("Socket path is too long: " + path_);
        }
        std::memcpy(addr.sun_path, path_.c_str(), path_.size());
        addr.sun_path[path_.size()] = '\0';

        const int connectResult =
            ::connect(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
#ifdef _WIN32
        if (connectResult == SOCKET_ERROR)
#else
        if (connectResult == -1)
#endif
        {
            const std::string error = ctrace::ipc::lastSocketError();
            close();
            throw ctrace::ipc::SocketError("Error connecting to socket '" + path_ + "': " + error);
        }
    }

    ~LocalSocketStrategy() override
    {
        close();
    }

    void write(const std::string& data) override
    {
        if (data.empty())
        {
            return;
        }

        if (!isOpen())
        {
            throw ctrace::ipc::SocketError("Socket is not connected");
        }

        std::size_t offset = 0;
        while (offset < data.size())
        {
#ifdef _WIN32
            const int sent =
                ::send(socket_, data.c_str() + offset, static_cast<int>(data.size() - offset), 0);
            if (sent == SOCKET_ERROR)
#else
            const auto sent = ::send(socket_, data.c_str() + offset, data.size() - offset, 0);
            if (sent == -1)
#endif
            {
                throw ctrace::ipc::SocketError("Failed to send IPC payload: " +
                                               ctrace::ipc::lastSocketError());
            }
            offset += static_cast<std::size_t>(sent);
        }
    }

    void close() override
    {
#ifdef _WIN32
        if (socket_ != INVALID_SOCKET)
        {
            ::closesocket(socket_);
            socket_ = INVALID_SOCKET;
        }
#else
        if (socket_ != -1)
        {
            ::close(socket_);
            socket_ = -1;
        }
#endif
    }

  private:
    [[nodiscard]] bool isOpen() const
    {
#ifdef _WIN32
        return socket_ != INVALID_SOCKET;
#else
        return socket_ != -1;
#endif
    }

    std::string path_;
#ifdef _WIN32
    SOCKET socket_ = INVALID_SOCKET;
#else
    int socket_ = -1;
#endif
};

#endif // IPC_STRATEGY_HPP
