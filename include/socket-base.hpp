#pragma once
#include <string>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <iostream>

class SocketBase
{
protected:
    int sockfd = -1;

public:
    SocketBase();
    explicit SocketBase(int fd);

    virtual ~SocketBase()
    {
        closeConnection();
    }

    void sendAll(const std::string &data);

    std::string receiveAll();

    std::string receiveSome(size_t maxSize);

    void closeConnection();

    int getFD() const;
};
