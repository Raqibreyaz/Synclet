#include "../include/tcp-socket.hpp"

TcpConnection::TcpConnection() = default;

TcpConnection::TcpConnection(const std::string &host, const std::string &port)
{
  connectToServer(host, port);
};

void TcpConnection::connectToServer(const std::string &host, const std::string &port)
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

TcpServer::TcpServer(const std::string &ip, const std::string &port)
{
  struct addrinfo hints{}, *res;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if (getaddrinfo(ip.c_str(), port.c_str(), &hints, &res) != 0)
    throw std::runtime_error("getaddrinfo failed");

  listen_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (listen_fd < 0)
    throw std::runtime_error("socket failed");

  int yes = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  if (bind(listen_fd, res->ai_addr, res->ai_addrlen) < 0)
    throw std::runtime_error("bind failed");

  if (listen(listen_fd, 10) < 0)
    throw std::runtime_error("listen failed");

  freeaddrinfo(res);
}

TcpConnection TcpServer::acceptClient()
{
  struct sockaddr_in clientAddr{};
  socklen_t len = sizeof(clientAddr);
  int client_fd = accept(listen_fd, (struct sockaddr *)&clientAddr, &len);
  if (client_fd < 0)
    throw std::runtime_error("accept failed");

  return TcpConnection(client_fd);
}

TcpServer::~TcpServer()
{
  if (listen_fd != -1)
    close(listen_fd);
}