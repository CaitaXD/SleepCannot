#ifndef MANAGEMENT_H_
#define MANAGEMENT_H_

#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <optional>

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

    constexpr bool operator==(const MacAddress &other) const
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

struct MachineEndpoint : IpEndPoint
{
    MacAddress mac;
    string hostname;

    MachineEndpoint() : IpEndPoint() {}
    MachineEndpoint(in_addr_t address, int port) : IpEndPoint(address, port) {}
    MachineEndpoint(struct sockaddr_in addr_in) : IpEndPoint(addr_in) {}

    MachineEndpoint with_port(int port) const
    {
        MachineEndpoint ep = *this;
        ep.addr_in.sin_port = htons(port);
        return ep;
    }

    MachineEndpoint with_address(uint32_t address) const
    {
        MachineEndpoint ep = *this;
        ep.addr_in.sin_addr.s_addr = htonl(address);
        return ep;
    }

    int get_port() const
    {
        return ntohs(((struct sockaddr_in *)&addr)->sin_port);
    }

    string to_string() const
    {
        auto ip = inet_ntoa(((struct sockaddr_in *)&addr)->sin_addr);
        auto port = ntohs(((struct sockaddr_in *)&addr)->sin_port);
        return hostname + " " + mac.mac_str + " " + string(ip) + ":" + std::to_string(port);
    }

    static MachineEndpoint MyMachine(Net::Adress address, int port)
    {
        MachineEndpoint ep;
        ep.addrlen = sizeof(struct sockaddr_in);
        ep.addr_in.sin_family = AdressFamily::InterNetwork;
        ep.addr_in.sin_addr.s_addr = address.network_order();
        ep.addr_in.sin_port = htons(port);
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
} participant_t;

// Represents the table of users using the service
struct ParticipantTable
{
    std::unordered_map<string, participant_t> map;
    bool dirty;
    std::mutex sync_root;

public:
    ParticipantTable();
    ~ParticipantTable();

    std::unique_lock<std::mutex> lock();
    void unlock();
    void print();
    void add(const participant_t &participant);
    void remove(const std::string &hostname);
    void update(const std::string &hostname, bool status);

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

// wol function
void wake_on_lan(int client_UdpSocket, participant_t participant)
{
    if (participant.machine.hostname == "None")
    {
        std::cout << "Participant not found" << std::endl;
        return;
    }
    // if (p.status) {
    //     std::cout << "Participant is already awake" << std::endl;
    //     return;
    // }
    // send packet
    std::string magic_packet = "wakeup " + participant.machine.hostname;
    int r = send(client_UdpSocket, magic_packet.c_str(), magic_packet.size(), 0);
    if (r < 0)
    {
        perror("wake_on_lan");
        return;
    }
}

ParticipantTable::ParticipantTable() : dirty(false){};
ParticipantTable::~ParticipantTable()
{
    unlock();
}

std::unique_lock<std::mutex> ParticipantTable::lock()
{
    std::unique_lock<std::mutex> lock(sync_root);
    return lock;
}

void ParticipantTable::unlock()
{
    sync_root.unlock();
}

void ParticipantTable::print()
{
    if (!dirty)
    {
        return;
    }
    std::cout << "\t\t\t\033[1mManagement Table\033[0m\t\t\t" << std::endl;
    std::cout << "\033[1mhostname\tmac address\t\tip\t\tstatus\033[0m" << std::endl;
    for (auto [host_name, participant] : map)
    {
        MachineEndpoint machine = participant.machine;
        char format_name = machine.hostname.size() < 8 ? '\t' : 0;
        std::cout << machine.to_string() << "\t" << format_name;
        std::cout << (participant.status ? "awaken" : "ASLEEP");
        std::cout << std::endl;
    }
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

void ParticipantTable::update(const std::string &hostname, bool status)
{
    if (map.find(hostname) == map.end())
    {
        return;
    }
    map[hostname].status = status;
    dirty = true;
}

participant_t &ParticipantTable::get(const std::string &hostname)
{
    return map.at(hostname);
}

#endif // MANAGEMENT_IMPLEMENTATION