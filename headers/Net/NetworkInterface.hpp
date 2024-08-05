#ifndef NETWORK_INTERFACE_H_
#define NETWORK_INTERFACE_H_

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
            msg += " UP ";
        if (flags & IFF_BROADCAST)
            msg += " BROADCAST ";
        if (flags & IFF_DEBUG)
            msg += " DEBUG ";
        if (flags & IFF_LOOPBACK)
            msg += " LOOPBACK ";
        if (flags & IFF_POINTOPOINT)
            msg += " POINTOPOINT ";
        if (flags & IFF_NOTRAILERS)
            msg += " NOTRAILERS ";
        if (flags & IFF_RUNNING)
            msg += " RUNNING ";
        if (flags & IFF_NOARP)
            msg += " NOARP ";
        if (flags & IFF_PROMISC)
            msg += " PROMISC ";
        if (flags & IFF_ALLMULTI)
            msg += " ALLMULTI ";
        if (flags & IFF_MASTER)
            msg += " MASTER ";
        if (flags & IFF_SLAVE)
            msg += " SLAVE ";
        if (flags & IFF_MULTICAST)
            msg += " MULTICAST ";
        if (flags & IFF_PORTSEL)
            msg += " PORTSEL ";
        if (flags & IFF_AUTOMEDIA)
            msg += " AUTOMEDIA ";
        if (flags & IFF_DYNAMIC)
            msg += " DYNAMIC ";

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

// Adapter for the ifaddrs linked list to a c++ iterator
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

#endif // NETWORK_INTERFACE_H_
#ifdef NETWORK_INTERFACE_IMPLEMENTATION

NetworkInterface::NetworkInterface(const ifaddrs &addrs)
{
    interface_name = addrs.ifa_name;
    flags = addrs.ifa_flags;
    network_address = addrs.ifa_addr;
    network_mask = addrs.ifa_netmask;
    memcpy(&interface_union, &addrs.ifa_ifu, sizeof(interface_union));
    interface_data = addrs.ifa_data;
}

#endif // NETWORK_INTERFACE_IMPLEMENTATION