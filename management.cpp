#include <iostream>
#include "management.hpp"

void print_participant(const participant_t& p) {
    std::cout << p.hostname << "\t\t";
    std::cout << p.mac.mac_str << "\t\t";
    std::cout << p.ip << "\t\t";
    std::cout << (p.status ? "awaken" : "ASLEEP") << "\t\t";
    std::cout << std::endl;
}

void print_management_table(const ParticipantTable& table) {
    // protect against multithreading
    std::cout << "hostname\tmac address\tip\t\tstatus" << std::endl;
    for (auto it = table.begin(); it != table.end(); ++it) {
        print_participant(it->second);
    }
}

void remove_participant(ParticipantTable& table, const std::string& hostname) {
    // protect against multithreading
    table.erase(hostname);
}

void add_participant(ParticipantTable& table, const participant_t& participant) {
    // protect against multithreading
    table[participant.hostname] = participant;
}

participant_t get_participant(const ParticipantTable& table, const std::string& hostname) {
    // protect against multithreading
    if (table.find(hostname) == table.end()) {
        participant_t p = {"None", {"None", "None"}, "None", false};
        return p;
    }
    return table.at(hostname);
}