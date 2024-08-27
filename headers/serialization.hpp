#ifndef SERIALIZATION_HPP_
#define SERIALIZATION_HPP_

#include <string>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <net/if.h>
#include "management.hpp"
#include <span>

static inline string serialize_participant(const Peer &participant)
{
    std::string stringified_table;
    std::string p_mac_addr(reinterpret_cast<const char *>(participant.machine.mac.mac_addr), sizeof(participant.machine.mac.mac_addr)); // unsigned char*
    std::string p_mac_str(reinterpret_cast<const char *>(participant.machine.mac.mac_str), sizeof(participant.machine.mac.mac_str));    // char*
    stringified_table += p_mac_addr + "\t";
    stringified_table += p_mac_str + "\t";
    stringified_table += std::to_string(((sockaddr_in *)&participant.machine.socket_address)->sin_port) + "\t"; // port
    stringified_table += inet_ntoa(((sockaddr_in *)&participant.machine.socket_address)->sin_addr);             // address
    stringified_table += "\t" + participant.machine.hostname + "\t";                                            // std::string
    stringified_table += std::to_string(participant.status) + "\t";                                             // bool
    stringified_table += std::to_string(participant.last_conection_timestamp) + "\t";                           // time_t
    stringified_table += std::to_string(participant.id) + "\t";                                                 // int
    stringified_table += std::to_string(participant.is_manager) + "\t";                                         // bool
    return stringified_table;
}

static inline string serialize_table(const ParticipantTable &participants)
{
    std::string stringified_table = "BEGIN TABLE\t" + std::to_string(participants.clock) + "\t";
    for (const auto &[host, participant] : participants)
    {
        stringified_table += serialize_participant(participant);
    }
    stringified_table += "END TABLE\t";
    return stringified_table;
}

// Modifes the span shifting the elements
static inline std::optional<Peer> deserialize_participant(std::span<string> &parts)
{
    int i = 0;
    const int participant_size = 9;
    try
    {
        if (parts.size() < participant_size)
        {
            LOGF("Participant size %zu is less than %d", parts.size(), participant_size);
            parts = parts.subspan(parts.size());
            return std::nullopt;
        }
        for (auto &part : parts)
        {
            if (std::addressof(part) == nullptr || part.data() == nullptr || part.empty())
            {
                parts = parts.subspan(participant_size);
                return std::nullopt;
            }
        }

        sockaddr_in ipv4 = {};
        MachineEndpoint recieved_machine{};
        memset(&ipv4, 0, sizeof(ipv4));
        ipv4.sin_family = AF_INET;

        memcpy(recieved_machine.mac.mac_addr, parts[i++].data(), MAC_ADDR_MAX); // add mac_addr (unsigned char*)
        memcpy(recieved_machine.mac.mac_str, parts[i++].data(), MAC_STR_MAX);   // add mac_str (char*)
        ipv4.sin_port = htons(std::stoi(parts[i++].data()));                           // port
        ipv4.sin_addr.s_addr = inet_addr(parts[i++].c_str());                   // address
        recieved_machine.hostname = parts[i++];                                 // hostname
        bool recieved_status = std::stoi(parts[i++]);                   // status
        time_t received_time_last = std::stol(parts[i++]);                      // last_conection_timestamp
        int recieved_id = std::stoi(parts[i++]);                                // identification
        bool recieved_is_manager = std::stoi(parts[i++]);               // add is_manager
        recieved_machine.socket_address = *(sockaddr *)&ipv4;
        assert("You did an upsie dupsie and potentialy a fucky wacky" && (i == participant_size));
        parts = parts.subspan(participant_size);
        return Peer{
            .machine = recieved_machine,
            .status = recieved_status,
            .client_socket = std::make_shared<Socket>(),
            .last_conection_timestamp = received_time_last,
            .id = recieved_id,
            .is_manager = recieved_is_manager};
    }
    catch (const std::exception &e)
    {
        parts = parts.subspan(participant_size);
    }
    return std::nullopt;
}

#ifdef SERIALIZATION_IMPLEMENTATION

#endif // SERIALIZATION_IMPLEMENTATION

#endif // SERIALIZATION_HPP_