#include "management.hpp"

// Display an individual participant
void print_participant(const participant_t& p) {
    std::cout << p.hostname << "\t\t";
    std::cout << p.mac.mac_str << "\t\t";
    std::cout << p.ip << "\t\t";
    std::cout << (p.status ? "awaken" : "ASLEEP") << "\t\t";
    std::cout << std::endl;
}

// Display the entire management table
void print_management_table(const ParticipantTable& table, mutex_data_t& mutex_data, int& read_count) {
    std::unique_lock<std::mutex> lock(mutex_data.mutex);
    mutex_data.cv.wait(lock, [&] { return read_count < mutex_data.update_count; });
    read_count = mutex_data.update_count;
    std::cout << "hostname\tmac address\tip\t\tstatus" << std::endl;
    for (auto it = table.begin(); it != table.end(); ++it) {
        print_participant(it->second);
    }
    mutex_data.updated = false;
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