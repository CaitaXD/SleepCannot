#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>
#include <condition_variable>
#include "Sockets.h"

#ifndef TCP_H_
#define TCP_H_

class TCP
{
public:
  TCP(int sockfd);
  TCP();
  ~TCP();
  int sockfd;
  int clientSocket;

  int socket();
  template <typename T>
  int set_option(int option, T value);
  template <typename T>
  int set_option(int option, T *value);

  // for client
  int connect(const std::string &ip, int port);
  int send(const std::string &data, int flags = 0);
  int recv(std::string *data);
  int close();

  // for server
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

TCP::TCP() : sockfd(-1) {}

TCP::TCP(int sockfd) : sockfd(sockfd) {}

TCP::~TCP()
{
  if (sockfd != -1)
  {
    ::close(sockfd);
  }
}

int TCP::socket()
{
  sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    perror("socket");
    return -1;
  }
  return 0;
}

template <typename T>
int TCP::set_option(int option, T value)
{
  if (::setsockopt(sockfd, SOL_SOCKET, option, &value, sizeof(value)) < 0)
  {
    perror("setsockopt");
    return -1;
  }
  return 0;
}

template <typename T>
int TCP::set_option(int option, T *value)
{
  if (::setsockopt(sockfd, SOL_SOCKET, option, value, sizeof(value)) < 0)
  {
    perror("setsockopt");
    return -1;
  }
  return 0;
}

int TCP::connect(const std::string &ip, int port)
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

int TCP::send(const std::string &data, int flags)
{
  ssize_t bytesSent = ::send(sockfd, data.c_str(), data.size(), flags);
  if (bytesSent < 0)
  {
    return -1;
  }
  return 0;
}

int TCP::recv(std::string *data)
{
  char buffer[1024] = {0};
  int bytesReceived = ::recv(sockfd, buffer, 1024, 0);
  if (bytesReceived < 0)
  {
    return -1;
  }
  *data = std::string(buffer, bytesReceived);
  return bytesReceived;
}

int TCP::close()
{
  if (::close(sockfd) < 0)
  {
    perror("close");
    return -1;
  }
  sockfd = -1;
  return 0;
}

int TCP::bind(int port)
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

int TCP::bind(EndPoint ep)
{
  if (::bind(sockfd, (struct sockaddr *)&ep.addr, ep.addrlen) < 0)
  {
    return -1;
  }
  return 0;
}

int TCP::listen(int backlog)
{
  if (::listen(sockfd, backlog) < 0)
  {
    perror("listen");
    return -1;
  }
  return 0;
}

int TCP::accept(EndPoint &ep)
{
  ep.addrlen = sizeof(struct sockaddr_in);
  bzero(&ep.addr, ep.addrlen);
  clientSocket = ::accept(sockfd, (struct sockaddr *)&ep.addr, &ep.addrlen);
  if (clientSocket < 0)
  {
    perror("accept");
  }
  return clientSocket;
}
#endif // TCP_H_