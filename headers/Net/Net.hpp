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

  struct Address
  {
    union
    {
      uint32_t netwrok_order_address;
      uint8_t bytes[4];
    };

    Address(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
    Address(const uint8_t adress[4]);
    Address(in_addr_t address);

    static Address parse(const string &ip);

    in_addr_t host_order() const;
    in_addr_t network_order() const;
  };

  struct IpEndpoint
  {
    socklen_t address_length;
    sockaddr socket_address;

    IpEndpoint();

    IpEndpoint(uint32_t address, int port);
    IpEndpoint(const string &ip, int port);
    IpEndpoint(sockaddr socket_adress);
    IpEndpoint(Address address, int port);
    IpEndpoint with_port(int port) const;
    IpEndpoint with_address(in_addr_t address) const;

    bool operator==(const IpEndpoint &other) const;
    static IpEndpoint broadcast(int port);
    string to_string() const;
  };

  struct NetworkInterface
  {
  public:
    NetworkInterface() = default;
    NetworkInterface(const ifaddrs &addrs);

    string_view interface_name;
    unsigned int flags;

    struct sockaddr *network_address;
    struct sockaddr *network_mask;
    union
    {
      struct sockaddr *broadcast_address;
      struct sockaddr *destination_address;
    } interface_union;
    void *interface_data;
#ifndef broadcast_address
#define broadcast_address interface_union.broadcast_address
#endif
#ifndef destination_address
#define destination_address interface_union.destination_address
#endif

    string to_string() const
    {
      string msg;
      msg += "NEWTWORK INTERFACE: " + string(interface_name) + "\n";
      if (network_address)
      {
        char buffer[INET_ADDRSTRLEN];
        msg += "ADDRESS: " + string(inet_ntop(AF_INET, &network_address, buffer, INET_ADDRSTRLEN)) + "\n";
      }
      if (network_mask)
      {
        char network_mask_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &network_mask, network_mask_str, INET_ADDRSTRLEN);
        msg += "MASK: " + string(network_mask_str) + "\n";
      }

      if (flags & IFF_UP)
      {
        msg += " UP ";
      }
      if (flags & IFF_BROADCAST)
      {
        msg += " BROADCAST ";
      }
      if (flags & IFF_DEBUG)
      {
        msg += " DEBUG ";
      }
      if (flags & IFF_LOOPBACK)
      {
        msg += " LOOPBACK ";
      }
      if (flags & IFF_POINTOPOINT)
      {
        msg += " POINTOPOINT ";
      }
      if (flags & IFF_NOTRAILERS)
      {
        msg += " NOTRAILERS ";
      }
      if (flags & IFF_RUNNING)
      {
        msg += " RUNNING ";
      }
      if (flags & IFF_NOARP)
      {
        msg += " NOARP ";
      }
      if (flags & IFF_PROMISC)
      {
        msg += " PROMISC ";
      }
      if (flags & IFF_ALLMULTI)
      {
        msg += " ALLMULTI ";
      }
      if (flags & IFF_MASTER)
      {
        msg += " MASTER ";
      }
      if (flags & IFF_SLAVE)
      {
        msg += " SLAVE ";
      }
      if (flags & IFF_MULTICAST)
      {
        msg += " MULTICAST ";
      }
      if (flags & IFF_PORTSEL)
      {
        msg += " PORTSEL ";
      }
      if (flags & IFF_AUTOMEDIA)
      {
        msg += " AUTOMEDIA ";
      }
      if (flags & IFF_DYNAMIC)
      {
        msg += " DYNAMIC ";
      }

      if (flags & IFF_BROADCAST)
      {
        char broadcast_address_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &broadcast_address, broadcast_address_str, INET_ADDRSTRLEN);

        msg += "Broadcast Address: " + string(broadcast_address_str) + "\n";
      }
      if (flags & IFF_POINTOPOINT)
      {
        char destination_address_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &destination_address, destination_address_str, INET_ADDRSTRLEN);
        msg += "Destination Address: " + string(destination_address_str) + "\n";
      }
      return msg;
    }
  };

  struct NetworkInterfaceList
  {
  public:
    NetworkInterfaceList(ifaddrs *addrs) : _ifaddrs(addrs), free(false) {}
    NetworkInterfaceList(ifaddrs &addrs) : _ifaddrs(std::addressof(addrs)), free(false) {}
    NetworkInterfaceList(ifaddrs &&addrs) : _ifaddrs(std::addressof(addrs)), free(true) {}
    ~NetworkInterfaceList()
    {
      if (free)
      {
        freeifaddrs(_ifaddrs);
      }
    }

    NetworkInterfaceList &operator=(ifaddrs *pNode)
    {
      this->_ifaddrs = pNode;
      return *this;
    }
    NetworkInterfaceList &operator++()
    {
      if (_ifaddrs)
        _ifaddrs = _ifaddrs->ifa_next;
      return *this;
    }
    NetworkInterfaceList operator++(int)
    {
      NetworkInterfaceList iterator = *this;
      ++*this;
      return iterator;
    }
    bool operator!=(const NetworkInterfaceList &iterator)
    {
      return _ifaddrs != iterator._ifaddrs;
    }

    const NetworkInterface &operator*()
    {
      _current = NetworkInterface(*_ifaddrs);
      return _current;
    }
    const NetworkInterface *operator->()
    {
      _current = NetworkInterface(*_ifaddrs);
      return &_current;
    }

    static NetworkInterfaceList begin()
    {
      return NetworkInterfaceList();
    }

    static NetworkInterfaceList end()
    {
      return NetworkInterfaceList(nullptr);
    }

  private:
    ifaddrs *_ifaddrs;
    NetworkInterface _current;
    bool free;
    NetworkInterfaceList() : _ifaddrs(nullptr), free(true)
    {
      if (getifaddrs(&_ifaddrs) == -1)
      {
        perror("getifaddrs");
        return;
      }
    }
  };
}
#endif // NET_H_
#ifdef NET_IMPLEMENTATION
namespace InternetAddress
{
  const Net::Address Any = Net::Address{htonl(INADDR_ANY)};
  const Net::Address Broadcast = Net::Address{htonl(INADDR_BROADCAST)};
  const Net::Address Loopback = Net::Address{htonl(INADDR_LOOPBACK)};
};
namespace Net
{
  Address::Address(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : bytes{a, b, c, d} {}
  Address::Address(const uint8_t host_order_adress[4]) { memmove(bytes, host_order_adress, 4); }
  Address::Address(in_addr_t host_order_adress) : netwrok_order_address(htonl(host_order_adress)) {}
  Address Address::parse(const string &ip)
  {
    auto addr = inet_addr(ip.c_str());
    return Address(addr);
  }

  in_addr_t Address::host_order() const { return ntohl(netwrok_order_address); }
  in_addr_t Address::network_order() const { return netwrok_order_address; }

  IpEndpoint::IpEndpoint()
  {
    address_length = sizeof(sockaddr);
  }

  IpEndpoint::IpEndpoint(uint32_t address, int port)
  {
    bzero(this, sizeof(*this));
    sockaddr_in *ipv4_socket_address = (sockaddr_in *)&socket_address;
    address_length = sizeof(*ipv4_socket_address);
    ipv4_socket_address->sin_family = AddressFamily::InterNetwork;
    ipv4_socket_address->sin_addr.s_addr = htonl(address);
    ipv4_socket_address->sin_port = htons(port);
  }

  IpEndpoint::IpEndpoint(const string &ip, int port)
  {
    bzero(this, sizeof(*this));
    sockaddr_in *ipv4_socket_address = (sockaddr_in *)&socket_address;
    address_length = sizeof(*ipv4_socket_address);
    ipv4_socket_address->sin_family = AddressFamily::InterNetwork;
    ipv4_socket_address->sin_addr.s_addr = inet_addr(ip.c_str());
    ipv4_socket_address->sin_port = htons(port);
  }

  IpEndpoint::IpEndpoint(sockaddr socket_adress) : address_length(sizeof(socket_adress)), socket_address(socket_adress) {}

  IpEndpoint::IpEndpoint(Address address, int port)
  {
    bzero(this, sizeof(*this));
    sockaddr_in *ipv4_socket_address = (sockaddr_in *)&socket_address;
    address_length = sizeof(*ipv4_socket_address);
    ipv4_socket_address->sin_family = AddressFamily::InterNetwork;
    ipv4_socket_address->sin_addr.s_addr = address.network_order();
    ipv4_socket_address->sin_port = htons(port);
  }

  IpEndpoint IpEndpoint::with_port(int port) const
  {
    IpEndpoint ep = *this;
    struct sockaddr_in *ipv4_socket_adress = (struct sockaddr_in *)&ep.socket_address;
    ep.address_length = sizeof(*ipv4_socket_adress);
    ipv4_socket_adress->sin_port = htons(port);
    return ep;
  }

  IpEndpoint IpEndpoint::with_address(in_addr_t address) const
  {
    IpEndpoint ep = *this;
    struct sockaddr_in *ipv4_socket_adress = (struct sockaddr_in *)&ep.socket_address;
    ep.address_length = sizeof(*ipv4_socket_adress);
    ipv4_socket_adress->sin_addr.s_addr = htonl(address);
    return ep;
  }

  bool IpEndpoint::operator==(const IpEndpoint &other) const
  {
    return address_length == other.address_length && memcmp(&socket_address, &other.socket_address, address_length) == 0;
  }

  IpEndpoint IpEndpoint::broadcast(int port)
  {
    return IpEndpoint(InternetAddress::Broadcast, port);
  }

  string IpEndpoint::to_string() const
  {
    sockaddr_in *ipv4_socket_address = (sockaddr_in *)&socket_address;
    string ip = inet_ntoa(ipv4_socket_address->sin_addr);
    string port = std::to_string(ipv4_socket_address->sin_port);
    return ip + ":" + port;
  }

  NetworkInterface::NetworkInterface(const ifaddrs &addrs)
  {
    interface_name = addrs.ifa_name;
    flags = addrs.ifa_flags;
    network_address = addrs.ifa_addr;
    network_mask = addrs.ifa_netmask;
    broadcast_address = addrs.ifa_broadaddr;
    destination_address = addrs.ifa_dstaddr;
    interface_data = addrs.ifa_data;
  }
}
#endif // NET_IMPLEMENTATION

using Address = Net::Address;
using IpEndpoint = Net::IpEndpoint;
using AddressFamily = Net::AddressFamily;
using SocketType = Net::SocketType;
using SocketProtocol = Net::SocketProtocol;
using NetworkInterface = Net::NetworkInterface;
using NetworkInterfaceList = Net::NetworkInterfaceList;