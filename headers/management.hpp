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
    int id; // used in election
    bool is_manager;
} participant_t;

// Represents the table of users using the service
struct ParticipantTable
{
    std::unordered_map<string, participant_t, StringHashIgnoreCase, StringEqComparerIgnoreCase> map;
    bool dirty;
    std::mutex sync_root;
    unsigned int clock;
    bool send_table;

    ParticipantTable();
    ~ParticipantTable();

    void lock();
    bool lock_if(std::function<bool(ParticipantTable &)> condition);
    void unlock();
    void print();
    void add(const participant_t &participant);
    void remove(const std::string &hostname);
    void update_status(const std::string &hostname, bool status);

    participant_t &get(const std::string &hostname);

    std::optional<participant_t *> find_by_socket(const Socket &socket);
    std::optional<participant_t *> find_by_address(const IpEndpoint &address);
    std::optional<participant_t *> find_by_id(int id);
    std::optional<participant_t *> find_manager();
    participant_t &find_by_address_blocking(const IpEndpoint &address);
    participant_t &find_manager_blocking();
    participant_t &find_by_id_blocking(int id);

    participant_t &get_or_add(const std::string &hostname, const participant_t &participant);
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

ParticipantTable::ParticipantTable() : map(), dirty(false), sync_root(), clock(0), send_table(false) {};
ParticipantTable::~ParticipantTable()
{
    unlock();
}

void ParticipantTable::lock()
{
    sync_root.lock();
}

bool ParticipantTable::lock_if(std::function<bool(ParticipantTable &)> condition)
{
    lock();
    bool result = condition(*this);
    if (!result)
    {
        unlock();
        return false;
    }
    return true;
}

void ParticipantTable::unlock()
{
    sync_root.unlock();
}

void ParticipantTable::print()
{
    std::cout << "\t\t\t\033[1mManagement Table\033[0m\t\t\t\n";
    std::cout << "\033[1mHost name\tMac address\t\tIp address\t\tstatus\t\tLast conection\033[0m\tId\n";
    for (auto [host_name, participant] : map)
    {
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

void ParticipantTable::add(const participant_t &participant)
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

participant_t &ParticipantTable::get(const std::string &hostname)
{
    return map.at(hostname);
}

std::optional<participant_t *> ParticipantTable::find_by_socket(const Socket &socket)
{
    for (auto &[host, participant] : map)
    {
        if (participant.socket->file_descriptor == socket.file_descriptor)
        {
            return std::addressof(map.at(host));
        }
    }
    return std::nullopt;
}

std::optional<participant_t *> ParticipantTable::find_by_address(const IpEndpoint &address)
{
    for (auto &[host, participant] : map)
    {
        sockaddr_in *ipv4_socket_address = (sockaddr_in *)&participant.machine.socket_address;
        sockaddr_in *peer_ipv4_socket_address = (sockaddr_in *)&address.socket_address;
        bool sockeq = memcmp(&ipv4_socket_address->sin_addr, &peer_ipv4_socket_address->sin_addr, sizeof(peer_ipv4_socket_address->sin_addr)) == 0;
        if (sockeq)
        {
            return std::addressof(map.at(host));
        }
    }
    return std::nullopt;
}

std::optional<participant_t *> ParticipantTable::find_by_id(int id)
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

participant_t &ParticipantTable::find_by_id_blocking(int id)
{
    auto opt = find_by_id(id);
    while (!opt.has_value())
    {
    }
    return *opt.value();
}

std::optional<participant_t *> ParticipantTable::find_manager()
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

participant_t &ParticipantTable::get_or_add(const std::string &hostname, const participant_t &participant)
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

participant_t &ParticipantTable::find_by_address_blocking(const IpEndpoint &address)
{
    auto opt = find_by_address(address);
    while (!opt.has_value())
    {
    }
    return *opt.value();
}

participant_t &ParticipantTable::find_manager_blocking()
{
    auto opt = find_manager();
    while (!opt.has_value())
    {
    }
    return *opt.value();
}

#endif // MANAGEMENT_IMPLEMENTATION
