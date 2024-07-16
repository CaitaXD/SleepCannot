#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>
#include <condition_variable>
#include "Sockets.h"

#ifndef Socket_H_
#define Socket_H_

class Socket
{
public:
  Socket(int sockfd);
  Socket();
  ~Socket();
  int sockfd;
  int client_socket;

  int open();
  template <typename T>
  int set_option(int option, T value);
  template <typename T>
  int set_option(int option, T *value);

  int connect(const std::string &ip, int port);
  int send(const std::string &payload, int flags = 0);
  int recv(std::string *payload, int flags = 0);
  int send(std::string &payload, EndPoint &ep, int flags = 0);
  int recv(std::string* payload, EndPoint &ep, int flags = 0);
  int close();
  int bind(int port);
  int bind(EndPoint ep);
  int listen(int backlog = 5);
  int accept(EndPoint &ep);

  int connect(EndPoint ep)
  {
    if (::connect(sockfd, (struct sockaddr *)&ep.addr, ep.addrlen) < 0)
    {
      return -1;
    }
    return 0;
  }
};

Socket::Socket() : sockfd(-1) {}

Socket::Socket(int sockfd) : sockfd(sockfd) {}

Socket::~Socket()
{
  if (sockfd != -1)
  {
    ::close(sockfd);
  }
}

int Socket::open()
{
  sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    perror("UdpSocket");
    return -1;
  }
  return 0;
}

template <typename T>
int Socket::set_option(int option, T value)
{
  if (::setsockopt(sockfd, SOL_SOCKET, option, &value, sizeof(value)) < 0)
  {
    perror("setsockopt");
    return -1;
  }
  return 0;
}

template <typename T>
int Socket::set_option(int option, T *value)
{
  if (::setsockopt(sockfd, SOL_SOCKET, option, value, sizeof(value)) < 0)
  {
    perror("setsockopt");
    return -1;
  }
  return 0;
}

int Socket::connect(const std::string &ip, int port)
{
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0)
  {
    perror("inet_pton");
    return -1;
  }

  if (::connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    perror("connect");
    return -1;
  }
  return 0;
}

int Socket::send(const std::string &payload, int flags)
{
  ssize_t bytesSent = ::send(sockfd, payload.c_str(), payload.size(), flags);
  if (bytesSent < 0)
  {
    return -1;
  }
  return 0;
}

int Socket::recv(std::string *payload, int flags)
{
  char buffer[1024] = {0};
  int bytesReceived = ::recv(sockfd, buffer, 1024, flags);
  if (bytesReceived < 0)
  {
    return -1;
  }
  *payload = std::string(buffer, bytesReceived);
  return bytesReceived;
}

int Socket::close()
{
  if (::close(sockfd) < 0)
  {
    perror("close");
    return -1;
  }
  sockfd = -1;
  return 0;
}

int Socket::bind(int port)
{
  struct sockaddr_in server_addr = {};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  if (::bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    return -1;
  }
  return 0;
}

int Socket::bind(EndPoint ep)
{
  if (::bind(sockfd, (struct sockaddr *)&ep.addr, ep.addrlen) < 0)
  {
    return -1;
  }
  return 0;
}

int Socket::listen(int backlog)
{
  if (::listen(sockfd, backlog) < 0)
  {
    perror("listen");
    return -1;
  }
  return 0;
}

int Socket::accept(EndPoint &ep)
{
  ep.addrlen = sizeof(struct sockaddr_in);
  bzero(&ep.addr, ep.addrlen);
  client_socket = ::accept(sockfd, (struct sockaddr *)&ep.addr, &ep.addrlen);
  if (client_socket < 0)
  {
    perror("accept");
  }
  return client_socket;
}

int Socket::recv(std::string *payload, EndPoint &ep, int flags)
{
  ep.addrlen = sizeof(sockaddr_in);
  char buffer[1024] = {0};
  int bytes_received = ::recvfrom(sockfd, buffer, 1024, flags, &ep.addr, &ep.addrlen);
  if (bytes_received < 0)
  {
    return -1;
  }
  *payload = std::string(buffer, bytes_received);
  return bytes_received;
}

int Socket::send(std::string &payload, EndPoint &ep, int flags)
{
  ep.addrlen = sizeof(sockaddr_in);
  return ::sendto(sockfd, payload.c_str(), payload.size(), flags, &ep.addr, ep.addrlen);
}
#endif // Socket_H_