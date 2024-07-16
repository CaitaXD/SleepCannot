#ifndef MANAGEMENT_IMPLEMENTATION
#define MANAGEMENT_IMPLEMENTATION

#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>
#include <condition_variable>
#include "commands.h"
#include "Socket.hpp"

// SHOULD NOT BE HERE, MOVE ELSEWHERE (i was having compiler issues)
// #define MAC_ADDR_MAX 6
// #define MAC_STR_MAX 64
// typedef struct {
//   unsigned char mac_addr[MAC_ADDR_MAX];
//   char mac_str[MAC_STR_MAX];
// } MacAddress;
// THIS

// Used to control read and write access to the management table
typedef struct mutex_data_t
{
    std::mutex mutex;
    std::condition_variable cv;
    bool updated; // might not be necessary
    int update_count;
    std::vector<int> read_count;
} mutex_data_t;

void print_participant(const participant_t& p);
void print_management_table(const ParticipantTable& table, mutex_data_t& mutex_data, int& read_count);
void add_participant(ParticipantTable& table, const participant_t& participant, mutex_data_t& mutex_data);
void remove_participant(ParticipantTable& table, const std::string& hostname, mutex_data_t& mutex_data);
void change_participant_state(ParticipantTable& table, const std::string& hostname, mutex_data_t& mutex_data);
participant_t get_participant(const ParticipantTable& table, const std::string& hostname, mutex_data_t& mutex_data, int& read_count);

// Display an individual participant
void print_participant(const participant_t& p) {
    char format_name = p.hostname.size() < 8 ? '\t' : '0';
    std::cout << p.hostname << "\t" << format_name;
    std::cout << p.mac.mac_str << "\t";
    std::cout << p.ip << "\t";
    std::cout << (p.status ? "awaken" : "ASLEEP");
    std::cout << std::endl;
}

// Display the entire management table
void print_management_table(const ParticipantTable& table, mutex_data_t& mutex_data, int& read_count) {
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_data.mutex);
        mutex_data.cv.wait(lock, [&] { return read_count < mutex_data.update_count; });
        read_count = mutex_data.update_count;
        std::cout << "\t\t\t\033[1mManagement Table\033[0m\t\t\t" << std::endl;
        std::cout << "\033[1mhostname\tmac address\t\tip\t\tstatus\033[0m" << std::endl;
        for (auto it = table.begin(); it != table.end(); ++it) {
            print_participant(it->second);
        }
        mutex_data.updated = false;
    }
}

// Insert a participant in the table
void add_participant(ParticipantTable& table, const participant_t& participant, mutex_data_t& mutex_data) {
    std::lock_guard<std::mutex> lock(mutex_data.mutex);
    table[participant.hostname] = participant;
    mutex_data.update_count++;
    mutex_data.cv.notify_all();
    mutex_data.updated = true;
}

// Removes a participant from the table by hostname
void remove_participant(ParticipantTable& table, const std::string& hostname, mutex_data_t& mutex_data) {
    std::lock_guard<std::mutex> lock(mutex_data.mutex);
    table.erase(hostname);
    mutex_data.update_count++;
    mutex_data.cv.notify_all();
    mutex_data.updated = true;
}

// Toggles status of participant by hostname
void change_participant_state(ParticipantTable& table, const std::string& hostname, mutex_data_t& mutex_data) {
    std::lock_guard<std::mutex> lock(mutex_data.mutex);
    if (table.find(hostname) == table.end()) {
        return;
    }
    table[hostname].status = !table[hostname].status; // toggle status
    mutex_data.update_count++;
    mutex_data.cv.notify_all();
    mutex_data.updated = true;
}

// Retrieves a participant by hostname
participant_t get_participant(const ParticipantTable& table, const std::string& hostname, mutex_data_t& mutex_data, int& read_count) {
    std::unique_lock<std::mutex> lock(mutex_data.mutex);
    mutex_data.cv.wait(lock, [&] { return read_count < mutex_data.update_count; });
    read_count = mutex_data.update_count;
    if (table.find(hostname) == table.end()) {
        participant_t p = {"None", {"None", "None"}, "None", false}; // If not found, return None participant
        return p;
    }
    mutex_data.updated = false;
    return table.at(hostname);
}

void show_status(const ParticipantTable& table, mutex_data_t& mutex_data, int& read_count) {
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_data.mutex);
        mutex_data.cv.wait(lock, [&] { return read_count < mutex_data.update_count; });
        read_count = mutex_data.update_count;
        for (auto it = table.begin(); it != table.end(); ++it) {
            if (it->second.status) {
                std::cout << it->first << " is awake" << std::endl;
            } else {
                std::cout << it->first << " is asleep" << std::endl;
            }
        }
        mutex_data.updated = false;
    }
}

//wol function
void wake_on_lan(int client_UdpSocket, participant_t participant) {
    if (participant.hostname == "None") {
        std::cout << "Participant not found" << std::endl;
        return;
    }
    // if (p.status) {
    //     std::cout << "Participant is already awake" << std::endl;
    //     return;
    // }
    // send packet
    std::string magic_packet = "wakeup " + participant.hostname;
    int r = send(client_UdpSocket, magic_packet.c_str(), magic_packet.size(), 0);
    if (r < 0) {
        perror("wake_on_lan");
        return;
    }
}


#endif // MANAGEMENT_IMPLEMENTATION