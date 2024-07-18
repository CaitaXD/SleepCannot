#ifndef NET_H_
#define NET_H_

#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>
#include <condition_variable>
#include "./../macros.h"

namespace Net
{
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

    Adress(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
    Adress(const uint8_t adress[4]);
    Adress(in_addr_t address);

    static Adress parse(const string &ip);

    in_addr_t host_order() const;
    in_addr_t network_order() const;
  };

  struct IpEndPoint
  {
    socklen_t addrlen;
    union
    {
      struct sockaddr addr;
      struct sockaddr_in addr_in;
    };

    IpEndPoint();

    IpEndPoint(uint32_t address, int port);
    IpEndPoint(const string &ip, int port);
    IpEndPoint(struct sockaddr_in addr_in);
    IpEndPoint with_port(int port) const;
    IpEndPoint with_address(in_addr_t address) const;

    bool operator==(const IpEndPoint &other) const;
    static IpEndPoint broadcast(int port);
    string to_string() const;
  };
}
#endif // NET_H_
#ifdef NET_IMPLEMENTATION
namespace InternetAdress
{
  const Net::Adress Any = Net::Adress{htonl(INADDR_ANY)};
  const Net::Adress Broadcast = Net::Adress{htonl(INADDR_BROADCAST)};
  const Net::Adress Loopback = Net::Adress{htonl(INADDR_LOOPBACK)};
};
namespace Net
{
  Adress::Adress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : bytes{a, b, c, d} {}
  Adress::Adress(const uint8_t host_order_adress[4]) { memmove(bytes, host_order_adress, 4); }
  Adress::Adress(in_addr_t host_order_adress) : netwrok_order_address(htonl(host_order_adress)) {}
  Adress Adress::parse(const string &ip)
  {
    auto addr = inet_addr(ip.c_str());
    return Adress(addr);
  }

  in_addr_t Adress::host_order() const { return ntohl(netwrok_order_address); }
  in_addr_t Adress::network_order() const { return netwrok_order_address; }

  IpEndPoint::IpEndPoint()
  {
    addrlen = sizeof(struct sockaddr_in);
  }

  IpEndPoint::IpEndPoint(uint32_t address, int port)
  {
    addrlen = sizeof(struct sockaddr_in);
    addr_in.sin_family = AdressFamily::InterNetwork;
    addr_in.sin_addr.s_addr = htonl(address);
    addr_in.sin_port = htons(port);
  }

  IpEndPoint::IpEndPoint(const string &ip, int port)
  {
    addrlen = sizeof(struct sockaddr_in);
    addr_in.sin_family = AdressFamily::InterNetwork;
    addr_in.sin_addr.s_addr = inet_addr(ip.c_str());
    addr_in.sin_port = htons(port);
  }

  IpEndPoint::IpEndPoint(struct sockaddr_in addr_in) : addrlen(sizeof(addr_in)), addr_in(addr_in) {}

  IpEndPoint IpEndPoint::with_port(int port) const
  {
    IpEndPoint ep = *this;
    ep.addrlen = sizeof(ep.addr);
    struct sockaddr_in *addr = (struct sockaddr_in *)&ep.addr;
    addr->sin_port = htons(port);
    return ep;
  }

  IpEndPoint IpEndPoint::with_address(in_addr_t address) const
  {
    IpEndPoint ep = *this;
    ep.addrlen = sizeof(ep.addr);
    struct sockaddr_in *addr = (struct sockaddr_in *)&ep.addr;
    addr->sin_addr.s_addr = htonl(address);
    return ep;
  }

  bool IpEndPoint::operator==(const IpEndPoint &other) const
  {
    return addrlen == other.addrlen && memcmp(&addr, &other.addr, addrlen) == 0;
  }

  IpEndPoint IpEndPoint::broadcast(int port)
  {
    return IpEndPoint(InternetAdress::Broadcast.network_order(), port);
  }

  string IpEndPoint::to_string() const
  {
    char buffer[INET_ADDRSTRLEN];
    inet_ntop(addr.sa_family, &addr.sa_data, buffer, INET_ADDRSTRLEN);
    return string(buffer);
  }
}
#endif // NET_IMPLEMENTATION

using Adress = Net::Adress;
using IpEndPoint = Net::IpEndPoint;
using AdressFamily = Net::AdressFamily;
using SocketType = Net::SocketType;
using SocketProtocol = Net::SocketProtocol;