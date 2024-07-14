#pragma once

#include <vector>
#include <unordered_map>
#include <string>

// SHOULD NOT BE HERE, MOVE ELSEWHERE (i was having compiler issues)
#define MAC_ADDR_MAX 6
#define MAC_STR_MAX 64
typedef struct {
  unsigned char mac_addr[MAC_ADDR_MAX];
  char mac_str[MAC_STR_MAX];
} MacAddress;
// THIS

// Represents a participant using the service
typedef struct participant_t
{
    std::string hostname;
    MacAddress mac;
    std::string ip;
    bool status; // true means awake, false means asleep
} participant_t;
// Represents the table of users using the service
typedef std::unordered_map<std::string, participant_t> ParticipantTable;

void print_participant(const participant_t& p);
void print_management_table(const ParticipantTable& table);
void remove_participant(ParticipantTable& table, const std::string& hostname);
void add_participant(ParticipantTable& table, const participant_t& participant);
participant_t get_participant(const ParticipantTable& table, const std::string& hostname);