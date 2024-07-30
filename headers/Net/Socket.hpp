/*
  Adpats the posix socket api to a more c++ like api
  This implementation does not declare copy constructors to avoid stale file descriptors and unwanted destruction on copies
*/

#ifndef SOCKET_H_
#define SOCKET_H_

#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>
#include <condition_variable>
#include <poll.h>
#include "Net.hpp"
#include "./../FileDescriptor.hpp"

struct Socket : FileDescriptor
{
  Socket(Socket &&other) : FileDescriptor(static_cast<FileDescriptor &&>(other)) {}
  Socket(int sockfd) : FileDescriptor(sockfd) {}
  Socket() : FileDescriptor() {}

public:
  int open(AddressFamily family, SocketType type, SocketProtocol protocol = SocketProtocol::NotSpecified);
  int open(SocketType type, SocketProtocol protocol = SocketProtocol::NotSpecified);
  template <typename T>
  int set_option(int option, T value);
  template <typename T>
  int set_option(int option, T *value);

  int connect(const string &ip, int port);
  int connect(const IpEndpoint &ep);
  int send(const string &payload, int flags = 0);
  int recv(string *payload, int flags = 0);
  int send(const string &payload, const IpEndpoint &ep, int flags = 0);
  int recv(string *payload, IpEndpoint &ep, int flags = 0);
  int close();
  int bind(int port);
  int bind(const IpEndpoint &ep);
  int bind(Address address, int port);
  int bind(string address, int port);
  int listen(int backlog = 5);
  Socket accept(IpEndpoint &ep);

  constexpr Socket &operator=(Socket &&other);
};

#endif // SOCKET_H_
#ifdef SOCKET_IMPLEMENTATION

FileDescriptor::FileDescriptor(FileDescriptor &&other)
{
  int fd = other.file_descriptor;
  other.file_descriptor = -1;
  file_descriptor = fd;

  keep_alive = other.keep_alive;
}

constexpr Socket &Socket::operator=(Socket &&other)
{
  int fd = other.file_descriptor;
  file_descriptor = fd;
  other.file_descriptor = -1;
  return *this;
}

int Socket::open(AddressFamily family, SocketType type, SocketProtocol protocol)
{
  file_descriptor = ::socket(family, type, protocol);
  return file_descriptor;
}

int Socket::open(SocketType type, SocketProtocol protocol)
{
  return open(AddressFamily::InterNetwork, type, protocol);
}

template <typename T>
int Socket::set_option(int option, T value)
{
  return ::setsockopt(file_descriptor, SOL_SOCKET, option, &value, sizeof(value));
}

template <typename T>
int Socket::set_option(int option, T *value)
{
  return ::setsockopt(file_descriptor, SOL_SOCKET, option, value, sizeof(*value));
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

  if (::connect(file_descriptor, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    return -1;
  }
  return 0;
}

int Socket::send(const string &payload, int flags)
{
  return ::send(file_descriptor, payload.c_str(), payload.size(), flags);
}

int Socket::recv(string *payload, int flags)
{
  char buffer[1024] = {0};
  int bytesReceived = ::recv(file_descriptor, buffer, 1024, flags);
  if (bytesReceived < 0)
  {
    return -1;
  }
  *payload = string(buffer, bytesReceived);
  return bytesReceived;
}

int Socket::close()
{
  if (::close(file_descriptor) < 0)
  {
    return -1;
  }
  file_descriptor = -1;
  return 0;
}

int Socket::bind(int port)
{
  struct sockaddr_in server_addr = {};
  server_addr.sin_family = AddressFamily::InterNetwork;
  server_addr.sin_addr.s_addr = InternetAddress::Any.network_order();
  server_addr.sin_port = htons(port);
  return ::bind(file_descriptor, (struct sockaddr *)&server_addr, sizeof(server_addr));
}

int Socket::bind(Address address, int port)
{
  struct sockaddr_in server_addr = {};
  server_addr.sin_family = AddressFamily::InterNetwork;
  server_addr.sin_addr.s_addr = address.network_order();
  server_addr.sin_port = htons(port);
  return ::bind(file_descriptor, (struct sockaddr *)&server_addr, sizeof(server_addr));
}

int Socket::bind(const IpEndpoint &ep)
{
  return ::bind(file_descriptor, (struct sockaddr *)&ep.socket_address, ep.address_length);
}

int Socket::bind(string address, int port)
{
  struct sockaddr_in ipv4_socket_address;
  bzero(&ipv4_socket_address, sizeof(ipv4_socket_address));
  ipv4_socket_address.sin_family = AddressFamily::InterNetwork;
  ipv4_socket_address.sin_addr.s_addr = inet_addr(address.c_str());
  ipv4_socket_address.sin_port = htons(port);
  return ::bind(file_descriptor, (struct sockaddr *)&ipv4_socket_address, sizeof(ipv4_socket_address));
}

int Socket::listen(int backlog)
{
  return ::listen(file_descriptor, backlog);
}

Socket Socket::accept(IpEndpoint &ep)
{
  int client_socket = ::accept(file_descriptor, (struct sockaddr *)&ep.socket_address, &ep.address_length);
  if (client_socket < 0)
  {
    return Socket{};
  }
  return Socket(client_socket);
}

int Socket::recv(string *payload, IpEndpoint &ep, int flags)
{
  char buffer[1024]{};
  int bytes_received = ::recvfrom(file_descriptor, buffer, 1024, flags, &ep.socket_address, &ep.address_length);
  if (bytes_received < 0)
  {
    return -1;
  }
  *payload = string(buffer, bytes_received);
  return bytes_received;
}

int Socket::send(const string &payload, const IpEndpoint &ep, int flags)
{
  return ::sendto(file_descriptor, payload.c_str(), payload.size(), flags, &ep.socket_address, ep.address_length);
}

int Socket::connect(const IpEndpoint &ep)
{
  return ::connect(file_descriptor, (struct sockaddr *)&ep.socket_address, ep.address_length);
}

#endif // SOCKET_IMPLEMENTATION