#ifndef IPC_STRATEGY_HPP
#define IPC_STRATEGY_HPP

#include <regex>

#include "../ProcessFactory.hpp"
#include "../ThreadProcess.hpp"

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
    virtual void close() = 0;  // Pour cleanup RAII
};

namespace ctrace::ipc
{
    class SocketError : public std::runtime_error
 {
    public:
        explicit SocketError(const std::string& msg)
            : std::runtime_error("[IPC::SocketError] " + msg) {}
    };
}

class UnixSocketStrategy : public IpcStrategy
{
private:
    int sock;
    std::string path;
public:
    UnixSocketStrategy(const std::string& socketPath) : path(socketPath), sock(-1) {
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock == -1) {
            throw std::runtime_error("Error socket creation: " + std::string(strerror(errno)));
        }
        sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == -1) {
            throw std::runtime_error("Error socket connexion: " + std::string(strerror(errno)));
        }
    }
    ~UnixSocketStrategy() { close(); }
    void write(const std::string& data) override
    {
        if (sock == -1)
        {
            throw ctrace::ipc::SocketError(std::string("Socket is not connected: ") + strerror(errno));
        }
        if (send(sock, data.c_str(), data.size(), 0) == -1)
        {
            throw ctrace::ipc::SocketError(std::string("Connection failed: ") + strerror(errno));
        }
    }
    void close() override {
        if (sock != -1) {
            ::close(sock);
            sock = -1;
            // unlink(path.c_str());  // Optionnel
        }
    }
};

#endif // IPC_STRATEGY_HPP
