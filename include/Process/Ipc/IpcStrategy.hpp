#ifndef IPC_STRATEGY_HPP
#define IPC_STRATEGY_HPP

#include <regex>

#include "../Tools/IAnalysisTools.hpp"
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

class UnixSocketStrategy : public IpcStrategy
{
private:
    int sock;
    std::string path;
public:
    UnixSocketStrategy(const std::string& socketPath) : path(socketPath), sock(-1) {
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock == -1) {
            throw std::runtime_error("Erreur création socket: " + std::string(strerror(errno)));
        }
        sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == -1) {
            throw std::runtime_error("Erreur connexion socket: " + std::string(strerror(errno)));
        }
    }
    ~UnixSocketStrategy() { close(); }
    void write(const std::string& data) override {
        if (send(sock, data.c_str(), data.size(), 0) == -1) {
            throw std::runtime_error("Erreur envoi socket: " + std::string(strerror(errno)));
        }
    }
    void close() override {
        if (sock != -1) {
            ::close(sock);
            sock = -1;
            unlink(path.c_str());  // Optionnel
        }
    }
};

#endif // IPC_STRATEGY_HPP
