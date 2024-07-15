#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "management.hpp"

int main(int argc, char** argv) {
    // std::mutex mutex;
    // std::condition_variable cv;
    // bool updated = false;
    // int read_count[2] = {0};
    // int update_count = 0;

    mutex_data_t mutex_data = {std::mutex(), std::condition_variable(), false, 0, {0}};
    std::thread writers[2];
    std::thread readers[2];
    mutex_data.read_count = std::vector<int>(2, 0);

    ParticipantTable participants;
    participant_t p1 = {"hopper", {"1:1:1", "1:1:1:1"}, "8.8.8.8", true};
    participant_t p2 = {"sagan", {"1:1:1", "1:1:1:1"}, "1.3.8.8", false};
    participant_t p3 = {"jonas", {"1:1:1", "1:1:1:1"}, "1.3.8.8", true};

    add_participant(participants, p1, mutex_data);

    writers[0] = std::thread(add_participant, std::ref(participants), p1, std::ref(mutex_data));
    writers[1] = std::thread(change_participant_state, std::ref(participants), "hopper", std::ref(mutex_data));
    
    readers[0] = std::thread(print_management_table, std::ref(participants), std::ref(mutex_data), std::ref(mutex_data.read_count[0]));
    readers[1] = std::thread(print_management_table, std::ref(participants), std::ref(mutex_data), std::ref(mutex_data.read_count[1]));

    for (int i = 0; i < 2; i++) {
        writers[i].join();
        readers[i].join();
    }

    // add_participant(participants, p1, mutex, updated, cv);
    // add_participant(participants, p2, mutex, updated, cv);
    //print_management_table(participants, mutex, updated, cv);
    
    //print_participant(get_participant(participants, "hopper"));
    // remove_participant(participants, "sagan");
    // print_management_table(participants);
    
    return 0;
}