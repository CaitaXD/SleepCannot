#ifndef SOCKET_H_
#define SOCKET_H_

#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>
#include <condition_variable>
#include "Net.hpp"

class Socket
{
public:
  Socket(Socket &&other);
  //Socket(const Socket &other);
  Socket(int sockfd);
  Socket();
  ~Socket();
  int sockfd;

  int open(AdressFamily family, SocketType type, SocketProtocol protocol = SocketProtocol::NotSpecified);
  template <typename T>
  int set_option(int option, T value);
  template <typename T>
  int set_option(int option, T *value);

  int connect(const string &ip, int port);
  int connect(const IpEndPoint &ep);
  int send(const string &payload, int flags = 0);
  int recv(string *payload, int flags = 0);
  int send(const string &payload, const IpEndPoint &ep, int flags = 0);
  int recv(string *payload, IpEndPoint &ep, int flags = 0);
  int close();
  int bind(int port);
  int bind(const IpEndPoint &ep);
  int bind(Adress address, int port);
  int listen(int backlog = 5);
  Socket accept(IpEndPoint &ep);

  constexpr Socket& operator=(Socket &&other);
  Socket & operator=(const Socket&) = delete;
};

#endif // SOCKET_H_
#ifdef SOCKET_IMPLEMENTATION

Socket::Socket(Socket &&other) : sockfd(other.sockfd)
{
  other.sockfd = -1;
}
//Socket::Socket(const Socket &other) : sockfd(other.sockfd) {}
Socket::Socket() : sockfd(-1) {}
Socket::Socket(int sockfd) : sockfd(sockfd) {}
Socket::~Socket()
{
  if (sockfd != -1)
  {
    ::close(sockfd);
  }
}

constexpr Socket& Socket::operator=(Socket &&other)
{
  sockfd = other.sockfd;
  other.sockfd = -1;
  return *this;
}

int Socket::open(AdressFamily family, SocketType type, SocketProtocol protocol)
{
  sockfd = ::socket(family, type, protocol);
  return sockfd;
}

template <typename T>
int Socket::set_option(int option, T value)
{
  return ::setsockopt(sockfd, SOL_SOCKET, option, &value, sizeof(value));
}

template <typename T>
int Socket::set_option(int option, T *value)
{
  return ::setsockopt(sockfd, SOL_SOCKET, option, value, sizeof(*value));
}

int Socket::connect(const string &ip, int port)
{
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0)
  {
    return -1;
  }

  if (::connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    return -1;
  }
  return 0;
}

int Socket::send(const string &payload, int flags)
{
  return ::send(sockfd, payload.c_str(), payload.size(), flags);
}

int Socket::recv(string *payload, int flags)
{
  char buffer[1024] = {0};
  int bytesReceived = ::recv(sockfd, buffer, 1024, flags);
  if (bytesReceived < 0)
  {
    return -1;
  }
  *payload = string(buffer, bytesReceived);
  return bytesReceived;
}

int Socket::close()
{
  if (::close(sockfd) < 0)
  {
    return -1;
  }
  sockfd = -1;
  return 0;
}

int Socket::bind(int port)
{
  struct sockaddr_in server_addr = {};
  server_addr.sin_family = AdressFamily::InterNetwork;
  server_addr.sin_addr.s_addr = InternetAdress::Any.network_order();
  server_addr.sin_port = htons(port);
  return ::bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
}

int Socket::bind(Adress address, int port)
{
  struct sockaddr_in server_addr = {};
  server_addr.sin_family = AdressFamily::InterNetwork;
  server_addr.sin_addr.s_addr = address.network_order();
  server_addr.sin_port = htons(port);
  return ::bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
}

int Socket::bind(const IpEndPoint &ep)
{
  return ::bind(sockfd, (struct sockaddr *)&ep.addr, ep.addrlen);
}

int Socket::listen(int backlog)
{
  return ::listen(sockfd, backlog);
}

Socket Socket::accept(IpEndPoint &ep)
{
  int client_socket = ::accept(sockfd, (struct sockaddr *)&ep.addr, &ep.addrlen);
  if (client_socket < 0)
  {
    return Socket(-1);
  }
  return Socket(client_socket);
}

int Socket::recv(string *payload, IpEndPoint &ep, int flags)
{
  char buffer[1024]{};
  int bytes_received = ::recvfrom(sockfd, buffer, 1024, flags, &ep.addr, &ep.addrlen);
  if (bytes_received < 0)
  {
    return -1;
  }
  *payload = string(buffer, bytes_received);
  return bytes_received;
}

int Socket::send(const string &payload, const IpEndPoint &ep, int flags)
{
  return ::sendto(sockfd, payload.c_str(), payload.size(), flags, &ep.addr, ep.addrlen);
}

int Socket::connect(const IpEndPoint &ep)
{
  return ::connect(sockfd, (struct sockaddr *)&ep.addr, ep.addrlen);
}

#endif // SOCKET_IMPLEMENTATION