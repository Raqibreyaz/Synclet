#pragma once
#include "./socket-base.hpp"
#include <iostream>

class TcpConnection : public SocketBase
{
public:
    TcpConnection();

    TcpConnection(const std::string &host, const std::string &port);

    explicit TcpConnection(int fd); // from accept()

    // removing copyable constructors
    TcpConnection(const TcpConnection &) = delete;
    TcpConnection &operator=(TcpConnection &) = delete;

    // adding movable constructors
    TcpConnection(TcpConnection &&other) noexcept;
    TcpConnection &operator=(TcpConnection &&other) noexcept;

    void connectToServer(const std::string &host, const std::string &port);
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