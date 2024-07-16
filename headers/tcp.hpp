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
  TCP();
  ~TCP();
  int sockfd;
  int clientSocket;

  int socket();

  // for client
  int connect(const std::string &ip, int port);
  int send(const std::string &data);
  int recv(std::string &data);
  int close();

  // for server
  int bind(int port);
  int listen(int backlog = 5);
  int accept(EndPoint &ep);

  int connect(EndPoint ep)
  {
    if (::connect(sockfd, (struct sockaddr *)&ep.addr, ep.addrlen) < 0)
    {
      perror("connect");
      return -1;
    }
    return 0;
  }
};

TCP::TCP() : sockfd(-1) {}

TCP::~TCP()
{
  if (sockfd != -1)
  {
    ::close(sockfd);
  }
  ::close(clientSocket);
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

int TCP::send(const std::string &data)
{
  std::cout << "Sending " << data.size() << " bytes" << std::endl;
  ssize_t bytesSent = ::send(sockfd, data.c_str(), data.size(), 0);
  std::cout << "Sent " << bytesSent << " bytes" << std::endl;
  if (bytesSent < 0)
  {
    perror("send");
    return -1;
  }
  return 0;
}

int TCP::recv(std::string &data)
{
  char buffer[1024] = {0};
  int bytesReceived = ::recv(sockfd, buffer, 1024, 0);
  if (bytesReceived < 0)
  {
    perror("recv");
    return -1;
  }
  memccpy(data.data(), buffer, 0, bytesReceived);
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
    perror("bind");
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
  int clientSocket = ::accept(sockfd, (struct sockaddr *)&ep.addr, &ep.addrlen);
  if (clientSocket < 0)
  {
    perror("accept");
    return -1;
  }
  return clientSocket;
}
#endif // TCP_H_