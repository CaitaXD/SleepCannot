/*
  Used for networking
  Adpats the posix socket api to a more c++ like api
*/

#ifndef NET_H_
#define NET_H_

#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>
#include <condition_variable>
#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <string.h>
#include <list>
#include <iterator>
#include <sys/ioctl.h>
#include <net/if.h>
#include "./../macros.h"

namespace Net
{
  enum AddressFamily
  {
    Unspecified = AF_UNSPEC,
    InterNetwork = AF_INET,
    IPv6 = AF_INET6
  };

  enum SocketType
  {
    Raw = SOCK_RAW,
    Stream = SOCK_STREAM,
    Datagram = SOCK_DGRAM,
    NonBlocking = SOCK_NONBLOCK
  };

  enum SocketProtocol
  {
    NotSpecified = 0,
    TCP = IPPROTO_TCP,
    UDP = IPPROTO_UDP
  };

  // Represents an internet address
  struct IpEndpoint
  {
    socklen_t address_length;
    union
    {
      sockaddr socket_address;
      sockaddr_in ipv4_socket_address;
      sockaddr_in6 ipv6_socket_address;
    };

    IpEndpoint();

    IpEndpoint(uint32_t address, int port);
    IpEndpoint(const string &ip, int port);
    IpEndpoint(AddressFamily family, const string &ip, int port);
    IpEndpoint(sockaddr socket_adress);
    IpEndpoint with_port(int port) const;

    constexpr AddressFamily family() const;

    bool operator==(const IpEndpoint &other) const;
    static IpEndpoint ipv4_broadcast(int port);
    static IpEndpoint ipv4_any(int port);
    static IpEndpoint ipv4_loopback(int port);
    string to_string() const;
  };
}
#endif // NET_H_
#ifdef NET_IMPLEMENTATION

namespace Net
{
  IpEndpoint::IpEndpoint() {}
  IpEndpoint::IpEndpoint(uint32_t address, int port)
  {
    address_length = sizeof(sockaddr_in);
    ipv4_socket_address.sin_family = AddressFamily::InterNetwork;
    ipv4_socket_address.sin_addr.s_addr = htonl(address);
    ipv4_socket_address.sin_port = htons(port);
  }

  IpEndpoint::IpEndpoint(const string &ip, int port)
  {
    struct addrinfo hints;
    struct addrinfo *result;
    hints.ai_family = AddressFamily::Unspecified;
    hints.ai_socktype = 0;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = SocketProtocol::NotSpecified;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    int rc = getaddrinfo(ip.c_str(), std::to_string(port).c_str(), &hints, &result);
    if (rc != 0)
    {
      throw std::runtime_error(gai_strerror(rc));
    }
    memcpy(&socket_address, result->ai_addr, result->ai_addrlen);
    address_length = result->ai_addrlen;
    freeaddrinfo(result);
  }

  IpEndpoint::IpEndpoint(AddressFamily family, const string &ip, int port)
  {
    if (family == AddressFamily::InterNetwork)
    {
      struct sockaddr_in *ipv4_socket_address = (sockaddr_in *)&socket_address;
      address_length = sizeof(*ipv4_socket_address);
      ipv4_socket_address->sin_family = family;
      ipv4_socket_address->sin_addr.s_addr = inet_addr(ip.c_str());
      ipv4_socket_address->sin_port = htons(port);
    }
    else if (family == AddressFamily::IPv6)
    {
      struct sockaddr_in6 *ipv6_socket_address = (sockaddr_in6 *)&socket_address;
      address_length = sizeof(*ipv6_socket_address);
      ipv6_socket_address->sin6_family = family;
      inet_pton(AF_INET6, ip.c_str(), &ipv6_socket_address->sin6_addr);
      ipv6_socket_address->sin6_port = htons(port);
    }
    else
    {
      struct addrinfo hints;
      struct addrinfo *result;
      hints.ai_family = family;
      hints.ai_socktype = 0;
      hints.ai_flags = AI_PASSIVE;
      hints.ai_protocol = SocketProtocol::NotSpecified;
      hints.ai_canonname = NULL;
      hints.ai_addr = NULL;
      hints.ai_next = NULL;
      int rc = getaddrinfo(ip.c_str(), std::to_string(port).c_str(), &hints, &result);
      if (rc != 0)
      {
        throw std::runtime_error(gai_strerror(rc));
      }
      memcpy(&socket_address, result->ai_addr, result->ai_addrlen);
      address_length = result->ai_addrlen;
      freeaddrinfo(result);
    }
  }

  IpEndpoint IpEndpoint::with_port(int port) const
  {
    IpEndpoint ep = *this;
    if (socket_address.sa_family == AddressFamily::InterNetwork)
    {
      struct sockaddr_in *ipv4_socket_adress = (struct sockaddr_in *)&ep.socket_address;
      ep.address_length = sizeof(*ipv4_socket_adress);
      ipv4_socket_adress->sin_port = htons(port);
      return ep;
    }
    else if (socket_address.sa_family == AddressFamily::IPv6)
    {
      struct sockaddr_in6 *ipv6_socket_adress = (struct sockaddr_in6 *)&ep.socket_address;
      ep.address_length = sizeof(*ipv6_socket_adress);
      ipv6_socket_adress->sin6_port = htons(port);
      return ep;
    }
    else
    {
      throw std::runtime_error("Unsupported address family");
    }
  }

  bool IpEndpoint::operator==(const IpEndpoint &other) const
  {
    return address_length == other.address_length && memcmp(&socket_address, &other.socket_address, address_length) == 0;
  }

  IpEndpoint IpEndpoint::ipv4_broadcast(int port)
  {
    return IpEndpoint(INADDR_BROADCAST, port);
  }

  IpEndpoint IpEndpoint::ipv4_any(int port)
  {
    return IpEndpoint(INADDR_ANY, port);
  }

  IpEndpoint IpEndpoint::ipv4_loopback(int port)
  {
    return IpEndpoint(INADDR_LOOPBACK, port);
  }

  string IpEndpoint::to_string() const
  {
    if (socket_address.sa_family == AddressFamily::InterNetwork)
    {
      struct sockaddr_in *ipv4_socket_address = (sockaddr_in *)&socket_address;
      string ip = inet_ntoa(ipv4_socket_address->sin_addr);
      string port = std::to_string(ipv4_socket_address->sin_port);
      return ip + ":" + port;
    }
    else if (socket_address.sa_family == AddressFamily::IPv6)
    {
      struct sockaddr_in6 *ipv6_socket_address = (sockaddr_in6 *)&socket_address;
      char buffer[INET6_ADDRSTRLEN];
      auto ip = inet_ntop(AF_INET6, &ipv6_socket_address->sin6_addr, buffer, INET6_ADDRSTRLEN);
      auto port = ntohs(ipv6_socket_address->sin6_port);
      return string(ip) + ":" + std::to_string(port);
    }
    else
    {
      return "Unsupported address family";
    }
  }

  constexpr AddressFamily IpEndpoint::family() const
  {
    return (AddressFamily)socket_address.sa_family;
  }
}
#endif // NET_IMPLEMENTATION

using IpEndpoint = Net::IpEndpoint;
using AddressFamily = Net::AddressFamily;
using SocketType = Net::SocketType;
using SocketProtocol = Net::SocketProtocol;