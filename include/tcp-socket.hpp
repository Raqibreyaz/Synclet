#pragma once
#include "./socket-base.hpp"

class TcpConnection : public SocketBase
{
public:
    TcpConnection();

    TcpConnection(const std::string &host, const std::string &port);

    void connectToServer(const std::string &host, const std::string &port)
    {
        struct addrinfo hints{}, *res;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0)
            throw std::runtime_error("getaddrinfo failed");

        for (auto p = res; p != nullptr; p = p->ai_next)
        {
            int tempfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (tempfd < 0)
                continue;

            if (connect(tempfd, p->ai_addr, p->ai_addrlen) == 0)
            {
                sockfd = tempfd;
                break;
            }

            close(tempfd);
        }

        freeaddrinfo(res);
        if (sockfd == -1)
            throw std::runtime_error("Failed to connect");
    }

    explicit TcpConnection(int fd) : SocketBase(fd) {} // from accept()
};

class TcpServer
{
private:
    int listen_fd = -1;

public:
    TcpServer(const std::string &ip, const std::string &port);
    TcpConnection acceptClient();

    ~TcpServer();
};