#ifndef MANAGEMENT_H_
#define MANAGEMENT_H_

#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <string.h>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <optional>
#include "Net/Socket.hpp"
#include "string_helpers.hpp"
#include <shared_mutex>

#define MAXLINE 1024
#define INITIAL_PORT 35512
#define DISCOVERY_PORT INITIAL_PORT + 0
#define TCP_SERVER_PORT INITIAL_PORT + 1
#define TCP_CLIENT_PORT INITIAL_PORT + 2

using string_view = std::string_view;
using string = std::string;

string client_msg = "General, Kenoby, you are a bold one";
string server_msg = "Hello there!";

#define MAC_ADDR_MAX 6
#define MAC_STR_MAX 64

const char *mac_paths[] = {
    "/sys/class/net/eth0/address",
    "/sys/class/net/enp0s3/address",
};

struct MacAddress
{
    unsigned char mac_addr[MAC_ADDR_MAX];
    char mac_str[MAC_STR_MAX];

    bool operator==(const MacAddress &other) const
    {
        return memcmp(mac_addr, other.mac_addr, MAC_ADDR_MAX) == 0;
    }

    static MacAddress get_mac()
    {
        MacAddress mac = {};

        FILE *f = NULL;
        for (size_t i = 0; i < ARRAY_LENGTH(mac_paths); i++)
        {
            f = fopen(mac_paths[i], "r");
            if (f != NULL)
            {
                break;
            }
        }
        if (f == NULL)
        {
            perror("get_mac");
            exit(EXIT_FAILURE);
        }

        char mac_str[MAC_STR_MAX];
        fscanf(f, "%s", mac_str);
        fclose(f);

        sscanf(mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac.mac_addr[0],
               &mac.mac_addr[1], &mac.mac_addr[2], &mac.mac_addr[3], &mac.mac_addr[4],
               &mac.mac_addr[5]);

        mac.mac_str[0] = '\0';
        for (int i = 0; i < MAC_ADDR_MAX; i++)
        {
            snprintf(mac.mac_str + strlen(mac.mac_str),
                     MAC_STR_MAX - strlen(mac.mac_str), "%02x%s", mac.mac_addr[i],
                     i < MAC_ADDR_MAX - 1 ? ":" : "");
        }
        return mac;
    }
};

struct MachineEndpoint : IpEndpoint
{
    MacAddress mac;
    string hostname;

    MachineEndpoint() : IpEndpoint() {}
    MachineEndpoint(in_addr_t address, int port) : IpEndpoint(address, port) {}
    MachineEndpoint(sockaddr socket_address) : IpEndpoint(socket_address) {}

    MachineEndpoint with_port(int port) const
    {
        MachineEndpoint ep = *this;
        struct sockaddr_in *ipv4_socket_address = (sockaddr_in *)&ep.socket_address;
        ipv4_socket_address->sin_port = htons(port);
        return ep;
    }

    MachineEndpoint with_address(uint32_t address) const
    {
        MachineEndpoint ep = *this;
        struct sockaddr_in *ipv4_socket_address = (sockaddr_in *)&ep.socket_address;
        ipv4_socket_address->sin_addr.s_addr = htonl(address);
        return ep;
    }

    int get_port() const
    {
        return ntohs(((struct sockaddr_in *)&socket_address)->sin_port);
    }

    string to_string() const
    {
        auto ip = inet_ntoa(((struct sockaddr_in *)&socket_address)->sin_addr);
        auto port = ntohs(((struct sockaddr_in *)&socket_address)->sin_port);
        return hostname + " " + mac.mac_str + " " + string(ip) + ":" + std::to_string(port);
    }

    static MachineEndpoint MyMachine(Address address, int port)
    {
        MachineEndpoint ep;
        sockaddr_in *ipv4_socket_address = (sockaddr_in *)&ep.socket_address;
        bzero(ipv4_socket_address, sizeof(*ipv4_socket_address));

        ep.address_length = sizeof(sockaddr_in);
        ipv4_socket_address->sin_family = AddressFamily::IPv4;
        ipv4_socket_address->sin_addr.s_addr = address.network_order();
        ipv4_socket_address->sin_port = htons(port);
        ep.mac = MacAddress::get_mac();
        ep.hostname = get_hostname();
        return ep;
    }
};

// Represents a participant using the service
typedef struct Peer
{
    MachineEndpoint machine;
    bool status; // true means awake, false means asleep
    std::shared_ptr<Socket> client_socket;
    time_t last_conection_timestamp;
    int id; // used in election
    bool is_manager;
} Peer;

// #include "DataStructures/ReaderWriterLock.h"

typedef std::shared_mutex Lock;
typedef std::unique_lock<Lock> WriteLock;
typedef std::shared_lock<Lock> ReadLock;
struct Dummy
{
    Lock &dummy;
    Dummy(Lock &lock) : dummy(lock) {}
};
//typedef Dummy WriteLock;
//typedef Dummy ReadLock;

// Represents the table of users using the service
struct ParticipantTable
{
private:
    std::unordered_map<string, Peer, StringHashIgnoreCase, StringEqComparerIgnoreCase> map;
    Lock sync_root;
    std::mutex upgrade_mutex;

public:
    bool dirty;
    unsigned int clock;
    bool send_table;

    ParticipantTable();

    void print();
    void add(const Peer &participant);
    void remove(const std::string &hostname);
    void update_status(const std::string &hostname, bool status);

    Peer &get(const std::string &hostname);

    std::optional<Peer *> find_socket(const Socket &socket);
    std::optional<Peer *> find_address(const IpEndpoint &address);
    std::optional<Peer *> find_id(int id);
    std::optional<Peer *> find_manager();

    Peer &find_address_blocking(const IpEndpoint &address);
    Peer &find_manager_blocking();
    Peer &find_id_blocking(int id);
    Peer &find_socket_blocking(const Socket &socket);
    Peer &get_or_add(const std::string &hostname, const Peer &participant);

    auto begin() { return map.begin(); }
    auto end() { return map.end(); }
    auto find(const std::string &hostname) { return map.find(hostname); }
    auto size() { return map.size(); }
    auto &operator[](const std::string &hostname) { return map[hostname]; }
    auto emplace(const std::string &hostname, const Peer &participant) { return map.emplace(hostname, participant); }

    const auto begin() const { return map.begin(); }
    const auto end() const { return map.end(); }
    const auto find(const std::string &hostname) const { return map.find(hostname); }
    const auto size() const { return map.size(); }

    ReadLock read_lock()
    {
        return ReadLock{sync_root};
    }

    WriteLock write_lock()
    {
        return WriteLock{sync_root};
    }
};

#endif // MANAGEMENT_H_
#ifdef MANAGEMENT_IMPLEMENTATION

ParticipantTable::ParticipantTable() : map(6), sync_root(), dirty(false), clock(0), send_table(false) {};

void ParticipantTable::print()
{
    std::cout << "\t\t\t\033[1mManagement Table\033[0m\t\t\t\n";
    std::cout << "\033[1mHost name\tMac address\t\tIp address\t\tstatus\t\tLast conection\t\tId\033[0m\n";
    for (auto [host_name, participant] : map)
    {
        if (participant.is_manager) continue;
        MachineEndpoint machine = participant.machine;
        string status = participant.status ? "awake" : "sleeping";
        struct tm *tm = localtime(&participant.last_conection_timestamp);
        std::printf("%s\t%s\t%s\t\t%s\t\t%d/%d/%d %d:%d.%d\t%d\n",
                    host_name.c_str(),
                    machine.mac.mac_str,
                    inet_ntoa(((sockaddr_in *)&machine.socket_address)->sin_addr),
                    status.c_str(),
                    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, participant.id);
    }
    std::cout << clock << std::endl;
    std::cout << std::endl;
    dirty = false;
}

void ParticipantTable::add(const Peer &participant)
{
    string machine_hostname = participant.machine.hostname;
    auto [_, success] = map.emplace(machine_hostname, participant);
    if (success)
    {
        clock++;
        send_table = true;
        dirty = true;
    }
}

void ParticipantTable::remove(const std::string &hostname)
{
    if (map.erase(hostname))
    {
        clock++;
        send_table = true;
        dirty = true;
    }
}

void ParticipantTable::update_status(const std::string &hostname, bool status)
{
    auto it = map.find(hostname);
    if (it == map.end())
    {
        return;
    }
    auto &[host, participant] = *it;
    if (participant.status != status)
    {
        participant.status = status;
        dirty = true;
    }
}

Peer &ParticipantTable::get(const std::string &hostname)
{
    return map.at(hostname);
}

std::optional<Peer *> ParticipantTable::find_socket(const Socket &socket)
{
    for (auto &[host, participant] : map)
    {
        if (participant.client_socket->file_descriptor == socket.file_descriptor)
        {
            return std::addressof(map.at(host));
        }
    }
    return std::nullopt;
}

std::optional<Peer *> ParticipantTable::find_address(const IpEndpoint &address)
{
    for (auto &[host, participant] : map)
    {
        sockaddr_in *ipv4_socket_address = (sockaddr_in *)&participant.machine.socket_address;
        sockaddr_in *needle = (sockaddr_in *)&address.socket_address;
        bool sockeq = strcmp(inet_ntoa(ipv4_socket_address->sin_addr), inet_ntoa(needle->sin_addr)) == 0;
        if (sockeq)
        {
            return std::addressof(map.at(host));
        }
    }
    return std::nullopt;
}

std::optional<Peer *> ParticipantTable::find_id(int id)
{
    for (auto &[host, participant] : map)
    {
        if (participant.id == id)
        {
            return std::addressof(map.at(host));
        }
    }
    return std::nullopt;
}

Peer &ParticipantTable::find_id_blocking(int id)
{
    auto opt = find_id(id);
    while (!opt.has_value())
    {
        opt = find_id(id);
    }
    return *opt.value();
}

std::optional<Peer *> ParticipantTable::find_manager()
{
    for (auto &[host, participant] : map)
    {
        if (participant.is_manager)
        {
            return std::addressof(map.at(host));
        }
    }
    return std::nullopt;
}

Peer &ParticipantTable::get_or_add(const std::string &hostname, const Peer &participant)
{
    auto it = map.find(hostname);
    if (it == map.end())
    {
        auto &ret = map[hostname] = participant;
        return ret;
    }
    else
    {
        return it->second;
    }
}

Peer &ParticipantTable::find_address_blocking(const IpEndpoint &address)
{
    auto opt = find_address(address);
    while (!opt.has_value())
    {
        opt = find_address(address);
    }
    return *opt.value();
}

Peer &ParticipantTable::find_manager_blocking()
{
    auto opt = find_manager();
    while (!opt.has_value())
    {
        opt = find_manager();
    }
    return *opt.value();
}

Peer &ParticipantTable::find_socket_blocking(const Socket &socket)
{
    auto opt = find_socket(socket);
    while (!opt.has_value())
    {
        opt = find_socket(socket);
    }
    return *opt.value();
}

#endif // MANAGEMENT_IMPLEMENTATION
