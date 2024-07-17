#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>
#include <condition_variable>

#ifndef Socket_H_
#define Socket_H_

enum AdressFamily
{
  InterNetwork = AF_INET,
  IPv6 = AF_INET6
};

enum SocketType
{
  Stream = SOCK_STREAM,
  Datagram = SOCK_DGRAM
};

enum SocketProtocol
{
  NotSpecified = 0,
  TCP = IPPROTO_TCP,
  UDP = IPPROTO_UDP
};

struct Adress
{
  union
  {
    uint32_t netwrok_order_address;
    uint8_t bytes[4];
  };

  Adress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : bytes{a, b, c, d} {}
  Adress(const uint8_t adress[4]) { memmove(bytes, adress, 4); }
  Adress(uint32_t address) : netwrok_order_address(htonl(address)) {}

  static Adress parse(const std::string &ip)
  {
    auto addr = inet_addr(ip.c_str());
    return Adress(addr);
  }

  uint32_t host_order() const { return ntohl(netwrok_order_address); }
  in_addr_t network_order() const { return netwrok_order_address; }
};

namespace InternetAdress
{
  const Adress Any = Adress{htonl(INADDR_ANY)};
  const Adress Broadcast = Adress{htonl(INADDR_BROADCAST)};
  const Adress Loopback = Adress{htonl(INADDR_LOOPBACK)};
}

typedef struct IpEndPoint
{
  socklen_t addrlen;
  union
  {
    struct sockaddr addr;
    struct sockaddr_in addr_in;
  };

  IpEndPoint()
  {
    addrlen = sizeof(struct sockaddr_in);
  }

  IpEndPoint(uint32_t address, int port)
  {
    addrlen = sizeof(struct sockaddr_in);
    addr_in.sin_family = AdressFamily::InterNetwork;
    addr_in.sin_addr.s_addr = htonl(address);
    addr_in.sin_port = htons(port);
  }

  IpEndPoint(const std::string &ip, int port)
  {
    addrlen = sizeof(struct sockaddr_in);
    addr_in.sin_family = AdressFamily::InterNetwork;
    addr_in.sin_addr.s_addr = inet_addr(ip.c_str());
    addr_in.sin_port = htons(port);
  }

  IpEndPoint(struct sockaddr_in addr_in) : addrlen(sizeof(addr_in)), addr_in(addr_in) {}

  IpEndPoint with_port(int port) const
  {
    IpEndPoint ep = *this;
    ep.addrlen = sizeof(ep.addr);
    struct sockaddr_in *addr = (struct sockaddr_in *)&ep.addr;
    addr->sin_port = htons(port);
    return ep;
  }

  IpEndPoint with_address(in_addr_t address) const
  {
    IpEndPoint ep = *this;
    ep.addrlen = sizeof(ep.addr);
    struct sockaddr_in *addr = (struct sockaddr_in *)&ep.addr;
    addr->sin_addr.s_addr = htonl(address);
    return ep;
  }

  bool operator==(const IpEndPoint &other) const
  {
    return addrlen == other.addrlen && memcmp(&addr, &other.addr, addrlen) == 0;
  }

  static IpEndPoint broadcast(int port)
  {
    return IpEndPoint(InternetAdress::Broadcast.network_order(), port);
  }
} IpEndPoint;

class Socket
{
public:
  Socket(int sockfd);
  Socket();
  ~Socket();
  int sockfd;
  int client_socket;

  int open(AdressFamily family, SocketType type, SocketProtocol protocol = SocketProtocol::NotSpecified);
  template <typename T>
  int set_option(int option, T value);
  template <typename T>
  int set_option(int option, T *value);

  int connect(const std::string &ip, int port);
  int connect(const IpEndPoint &ep);
  int send(const std::string &payload, int flags = 0);
  int recv(std::string *payload, int flags = 0);
  int send(const std::string &payload, const IpEndPoint &ep, int flags = 0);
  int recv(std::string *payload, IpEndPoint &ep, int flags = 0);
  int close();
  int bind(int port);
  int bind(const IpEndPoint &ep);
  int bind(Adress address, int port);
  int listen(int backlog = 5);
  int accept(IpEndPoint &ep);
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

int Socket::connect(const std::string &ip, int port)
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

int Socket::send(const std::string &payload, int flags)
{
  return ::send(sockfd, payload.c_str(), payload.size(), flags);
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
    return -1;
  }
  sockfd = -1;
  return 0;
}

int Socket::bind(int port)
{
  struct sockaddr_in server_addr = {};
  server_addr.sin_family = AdressFamily::InterNetwork;
  server_addr.sin_addr.s_addr = INADDR_ANY;
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

int Socket::accept(IpEndPoint &ep)
{
  return ::accept(sockfd, (struct sockaddr *)&ep.addr, &ep.addrlen);
}

int Socket::recv(std::string *payload, IpEndPoint &ep, int flags)
{
  char buffer[1024]{};
  int bytes_received = ::recvfrom(sockfd, buffer, 1024, flags, &ep.addr, &ep.addrlen);
  if (bytes_received < 0)
  {
    return -1;
  }
  *payload = std::string(buffer, bytes_received);
  return bytes_received;
}

int Socket::send(const std::string &payload, const IpEndPoint &ep, int flags)
{
  return ::sendto(sockfd, payload.c_str(), payload.size(), flags, &ep.addr, ep.addrlen);
}

int Socket::connect(const IpEndPoint &ep)
{
  return ::connect(sockfd, (struct sockaddr *)&ep.addr, ep.addrlen);
}
#endif // Socket_H_