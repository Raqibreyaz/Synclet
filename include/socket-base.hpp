#pragma once
#include <string>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>

class SocketBase
{
protected:
    int sockfd = -1;

public:
    SocketBase() = default;
    explicit SocketBase(int fd) : sockfd(fd) {}

    virtual ~SocketBase()
    {
        closeConnection();
    }

    void sendAll(const std::string &data)
    {
        size_t totalSent = 0;
        while (totalSent < data.size())
        {
            ssize_t sent = send(sockfd, data.c_str() + totalSent, data.size() - totalSent, 0);
            if (sent < 0)
                throw std::runtime_error("send failed");
            totalSent += sent;
        }
    }

    std::string receiveAll()
    {
        char buffer[1024];
        std::string result;
        ssize_t received = 0;
        while ((received = recv(sockfd, buffer, sizeof(buffer), 0)) > 0)
        {
            result.append(buffer, received);
        }
        if (received < 0)
            throw std::runtime_error("recv failed");

        return result;
    }

    std::string receiveSome(size_t maxSize = 1024)
    {
        char buffer[1024];
        std::string result;
        ssize_t received = recv(sockfd, buffer, maxSize, 0);
        if (received < 0)
            throw std::runtime_error("recv failed");
        result.append(buffer, received);
        return result;
    }

    void closeConnection()
    {
        if (sockfd != -1)
        {
            close(sockfd);
            sockfd = -1;
        }
    }

    int getFD() const { return sockfd; }
};
