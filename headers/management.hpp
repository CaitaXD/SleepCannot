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

// Used to control read and write access to the management table
typedef struct mutex_data_t
{
    std::mutex mutex;
    std::condition_variable cv;
    bool updated; // might not be necessary
    int update_count;
    std::vector<int> read_count;
} mutex_data_t;

#define MAXLINE 1024
#define INITIAL_PORT 35512

using string_view = std::string_view;
using string = std::string;

string client_msg = "General, Kenoby, you are a bold one";
string server_msg = "Hello there!";

#define MAC_ADDR_MAX 6
#define MAC_STR_MAX 64
#define MAC_ADDRES_FILE "/sys/class/net/eth0/address"

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

        FILE *f = fopen(MAC_ADDRES_FILE, "r");
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

        ep.address_length = sizeof(struct sockaddr_in);
        ipv4_socket_address->sin_family = AddressFamily::InterNetwork;
        ipv4_socket_address->sin_addr.s_addr = address.network_order();
        ipv4_socket_address->sin_port = htons(port);
        ep.mac = MacAddress::get_mac();
        ep.hostname = get_hostname();
        return ep;
    }
};

// Represents a participant using the service
typedef struct participant_t
{
    MachineEndpoint machine;
    bool status; // true means awake, false means asleep
    std::shared_ptr<Socket> socket;
    time_t last_conection_timestamp;
} participant_t;

// Represents the table of users using the service
struct ParticipantTable
{
    std::unordered_map<string, participant_t, StringHashIgnoreCase, StringEqComparerIgnoreCase> map;
    bool dirty;
    std::mutex sync_root;

    ParticipantTable();
    ~ParticipantTable();

    void lock();
    void unlock();
    void print();
    void add(const participant_t &participant);
    void remove(const std::string &hostname);
    void update_status(const std::string &hostname, bool status);

    participant_t &get(const std::string &hostname);
};

#endif // MANAGEMENT_H_
#ifdef MANAGEMENT_IMPLEMENTATION

void show_status(const std::unordered_map<string, participant_t> &table, mutex_data_t &mutex_data, int &read_count)
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(mutex_data.mutex);
        mutex_data.cv.wait(lock, [&]
                           { return read_count < mutex_data.update_count; });
        read_count = mutex_data.update_count;
        for (auto it = table.begin(); it != table.end(); ++it)
        {
            if (it->second.status)
            {
                std::cout << it->first << " is awake" << std::endl;
            }
            else
            {
                std::cout << it->first << " is asleep" << std::endl;
            }
        }
        mutex_data.updated = false;
    }
}

ParticipantTable::ParticipantTable() : map(), dirty(false), sync_root(){};
ParticipantTable::~ParticipantTable()
{
    unlock();
}

void ParticipantTable::lock()
{
    sync_root.lock();
}

void ParticipantTable::unlock()
{
    sync_root.unlock();
}

void ParticipantTable::print()
{
    std::cout << "\t\t\t\033[1mManagement Table\033[0m\t\t\t\n";
    std::cout << "\033[1mHost name\tMac address\t\tIp address\t\tstatus\t\tLast conection\033[0m\n";
    for (auto [host_name, participant] : map)
    {
        MachineEndpoint machine = participant.machine;
        string status = participant.status ? "awake" : "sleeping";
        struct tm *tm = localtime(&participant.last_conection_timestamp);
        std::printf("%s\t%s\t%s\t\t%s\t\t%d/%d/%d %d:%d.%d\n",
                    host_name.c_str(),
                    machine.mac.mac_str,
                    inet_ntoa(((sockaddr_in *)&machine.socket_address)->sin_addr),
                    status.c_str(),
                    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
    }
    std::cout << std::endl;
    dirty = false;
}

void ParticipantTable::add(const participant_t &participant)
{
    auto [_, success] = map.emplace(participant.machine.hostname, participant);
    if (success)
    {
        dirty = true;
    }
}

void ParticipantTable::remove(const std::string &hostname)
{
    if (map.erase(hostname))
    {
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

participant_t &ParticipantTable::get(const std::string &hostname)
{
    return map.at(hostname);
}

#endif // MANAGEMENT_IMPLEMENTATION
