#pragma once
#include "./socket-base.hpp"
#include <iostream>

class TcpConnection : public SocketBase
{
public:
    TcpConnection();

    TcpConnection(const std::string &host, const std::string &port);

    void connectToServer(const std::string &host, const std::string &port);

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