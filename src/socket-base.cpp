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
            throw std::runtime_error(std::string("send failed: ") + strerror(errno));
        totalSent += sent;
    }
}

std::string SocketBase::receiveAll()
{
    constexpr size_t BUFFER_SIZE = 1024;
    std::string result;
    std::string received_data;

    // receive data till we get empty response
    while (!((received_data = this->receiveSome(BUFFER_SIZE)).empty()))
        result.append(received_data);

    return result;
}
std::string SocketBase::receiveSome(const size_t maxSize = 1024)
{
    std::string result(maxSize, '\0');
    ssize_t total_received_bytes = 0, received_bytes = 0;

    do
    {
        received_bytes = recv(sockfd,
                              result.data() + total_received_bytes,
                              result.size() - total_received_bytes,
                              0);

        if (received_bytes < 0)
            throw std::runtime_error(std::string("recv failed: ") + std::strerror(errno));

        total_received_bytes += received_bytes;

    } while (total_received_bytes < received_bytes);

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

void SocketBase::shutdownWrite()
{
    if (sockfd != -1)
        shutdown(sockfd, SHUT_WR);
}

int SocketBase::getFD() const { return sockfd; }
