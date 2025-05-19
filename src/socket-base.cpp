#include "../include/socket-base.hpp"

SocketBase::SocketBase() = default;

SocketBase::SocketBase(int fd) : sockfd(fd) {}

void SocketBase::sendAll(const std::string &data)
{
    size_t totalSent = 0;
    while (totalSent < data.size())
    {
        ssize_t sent = send(sockfd, data.c_str() + totalSent, data.size() - totalSent, 0);
        if (sent < 0)
            throw std::runtime_error("send failed");
        totalSent += sent;
    }
    // shutting send for EOF
    shutdown(sockfd, SHUT_WR);
}

std::string SocketBase::receiveAll()
{
    constexpr size_t BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    std::string result;
    ssize_t received = 0;

    while ((received = recv(sockfd, buffer, BUFFER_SIZE, 0)) > 0)
    {
        result.append(buffer, received);
    }

    if (received == 0)
    {
        std::clog << "Connection closed by peer." << std::endl;
    }
    else if (received < 0)
    {
        throw std::runtime_error(std::string("recv failed: ") + std::strerror(errno));
    }
    return result;
}
std::string SocketBase::receiveSome(size_t maxSize = 1024)
{
    char buffer[1024];
    std::string result;
    ssize_t received = recv(sockfd, buffer, maxSize, 0);
    if (received < 0)
        throw std::runtime_error("recv failed");
    result.append(buffer, received);
    return result;
}

void SocketBase::closeConnection()
{
    if (sockfd != -1)
    {
        close(sockfd);
        sockfd = -1;
    }
}

int SocketBase::getFD() const { return sockfd; }
