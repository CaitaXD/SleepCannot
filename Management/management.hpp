#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>
#include <condition_variable>

// SHOULD NOT BE HERE, MOVE ELSEWHERE (i was having compiler issues)
#define MAC_ADDR_MAX 6
#define MAC_STR_MAX 64
typedef struct {
  unsigned char mac_addr[MAC_ADDR_MAX];
  char mac_str[MAC_STR_MAX];
} MacAddress;
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
void print_management_table(const ParticipantTable& table, mutex_data_t& mutex_data, int& read_count);
void add_participant(ParticipantTable& table, const participant_t& participant, mutex_data_t& mutex_data);
void remove_participant(ParticipantTable& table, const std::string& hostname, mutex_data_t& mutex_data);
void change_participant_state(ParticipantTable& table, const std::string& hostname, mutex_data_t& mutex_data);
participant_t get_participant(const ParticipantTable& table, const std::string& hostname, mutex_data_t& mutex_data, int& read_count);