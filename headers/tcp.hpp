#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>
#include <condition_variable>
#include "Sockets.h"

class TCP
{
public:
  TCP();
  ~TCP();

  // for client
  int connect(const std::string &ip, int port);
  int send(const std::string &data);
  int recv(std::string &data);
  int close();
  int socket();

  // for server
  int bind(int port);
  int listen(int backlog = 5);
  int accept();
  int send(int clientSocket, const std::string &data);
  int recv(int clientSocket, std::string &data);
  int close(int clientSocket);
  int socket();

private:
  int sockfd;
  std::vector<int> clientSockets;
};

TCP::TCP() : sockfd(-1) {}

TCP::~TCP() {
  if (sockfd != -1) {
    ::close(sockfd);
  }
  for (int clientSocket : clientSockets) {
    ::close(clientSocket);
  }
}

int TCP::socket() {
  sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    std::cerr << "Error creating socket" << std::endl;
    return -1;
  }
  return 0;
}

int TCP::connect(const std::string &ip, int port) {
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
    std::cerr << "Invalid address/ Address not supported" << std::endl;
    return -1;
  }

  if (::connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    std::cerr << "Connection Failed" << std::endl;
    return -1;
  }
  return 0;
}

int TCP::send(const std::string &data) {
  if (::send(sockfd, data.c_str(), data.size(), 0) < 0) {
    std::cerr << "Send failed" << std::endl;
    return -1;
  }
  return 0;
}

int TCP::recv(std::string &data) {
  char buffer[1024] = {0};
  int bytesReceived = ::recv(sockfd, buffer, 1024, 0);
  if (bytesReceived < 0) {
    std::cerr << "Receive failed" << std::endl;
    return -1;
  }
  data = std::string(buffer, bytesReceived);
  return bytesReceived;
}

int TCP::close() {
  if (::close(sockfd) < 0) {
    std::cerr << "Close failed" << std::endl;
    return -1;
  }
  sockfd = -1;
  return 0;
}

int TCP::bind(int port) {
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  if (::bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    std::cerr << "Bind failed" << std::endl;
    return -1;
  }
  return 0;
}

int TCP::listen(int backlog) {
  if (::listen(sockfd, backlog) < 0) {
    std::cerr << "Listen failed" << std::endl;
    return -1;
  }
  return 0;
}

int TCP::accept() {
  int clientSocket = ::accept(sockfd, nullptr, nullptr);
  if (clientSocket < 0) {
    std::cerr << "Accept failed" << std::endl;
    return -1;
  }
  clientSockets.push_back(clientSocket);
  return clientSocket;
}

int TCP::send(int clientSocket, const std::string &data) {
  if (::send(clientSocket, data.c_str(), data.size(), 0) < 0) {
    std::cerr << "Send to client failed" << std::endl;
    return -1;
  }
  return 0;
}

int TCP::recv(int clientSocket, std::string &data) {
  char buffer[1024] = {0};
  int bytesReceived = ::recv(clientSocket, buffer, 1024, 0);
  if (bytesReceived < 0) {
    std::cerr << "Receive from client failed" << std::endl;
    return -1;
  }
  data = std::string(buffer, bytesReceived);
  return bytesReceived;
}

int TCP::close(int clientSocket) {
  if (::close(clientSocket) < 0) {
    std::cerr << "Close client socket failed" << std::endl;
    return -1;
  }
  for (auto it = clientSockets.begin(); it != clientSockets.end(); ++it) {
    if (*it == clientSocket) {
      clientSockets.erase(it);
      break;
    }
  }

  return 0;
}
