#ifndef ELECTION_H_
#define ELECTION_H_

#include <iostream>
#include <vector>
#include <mutex>
#include <string>
#include "management.hpp"
#include "Net/Socket.hpp"
#include "macros.h"
#include "node.hpp"

#define TIMEOUT_ELECTION 2500 // ms
#define TIMEOUT_COORDINATOR 5000 // ms

/*
    Assumptions:
    process fails by stopping and returns from failure by restarting.
    there is a failure detector which detects failed processes.
    message delivery between processes is reliable.
    each process knows its own process id and address, and that of every other process.
    each process has a unique priority number.
    all processes in the system are fully connected.
    
    Goals:
    the process with the highest priority number will be elected as coordinator.
    there should be only one leader among the processes.
    all Processes agree on who is the leader.

    Election message format:
    E<op><id>
    E: election message
    op: operation (e for election, a for answer, c for coordinator)
    id: node id
*/

namespace Election {
    void run_election(Node& node); // starts election process
    void send_election(Node& node); // sends election message to all participants with higher id
    void answer_election(Node& node); // answers election message
    void send_coordinator(Node& node); // sends coordinator message to all participants
    // TODO: implement on monitoring maybe
    bool should_run_election(); // determined in monitoring service
    bool check_reply_from_election(Node& node); // check if there is a reply from election message
    int check_coordinator(Node& node); // check if there is a coordinator message and returns its id (or -1 if no coordinator)
} 

#endif ELECTION_H_

#ifndef ELECTION_IMPLEMENTATION
#define ELECTION_IMPLEMENTATION

// Send coordinator message to all participants
void Election::send_coordinator(Node& node) {
    node.participants.lock();
    for (auto &[host, participant] : node.participants.map) { // might need to use map.at(host) instead of participant
        participant.socket->send("Ec" + std::to_string(node.info.id));
    }
    node.participants.unlock();
}

// Send election message to participants with higher id
void Election::send_election(Node& node) {
    node.participants.lock();
    for (auto &[host, participant] : node.participants.map) { // might need to use map.at(host) instead of participant
        if (node.info.id < participant.id) {
            participant.socket->send("Ee" + std::to_string(node.info.id));
        }
    }
    node.participants.unlock();
}

// run in thread (maybe on listen, which might be on monitoring service)
// Answers election message of lower id node and starts election if it has not started
void Election::answer_election(Node& node, int sender_id) {
    if (node.info.id > sender_id) {
        node.participants.lock();
        for (auto &[host, participant] : node.participants.map) { // might need to use map.at(host) instead of participant
            if (participant.id != sender_id) continue;
            participant.socket->send("Ea" + std::to_string(node.info.id));
            if (!node.has_started_election) Election::run_election(node);
            node.participants.unlock();
            return;
        }
        node.participants.unlock();
    }
}

// run in thread
// Starts election process
void Election::run_election(Node& node) {
    restart_election:
    if (node.has_started_election) return;
    node.has_started_election = true;
    // Sends coordinator message if it has the highest id
    bool highest_id = true;
    node.participants.lock();
    for (auto &[host, participant] : node.participants.map) {
        if (!participant.status) continue;
        if (node.info.id < participant.id) {
            highest_id = false;
            break;
        }
    }
    node.participants.unlock();
    if (highest_id) {
        Election::send_coordinator(node);
        node.manager_id = node.info.id;
        node.has_started_election = false;
        return;
    }
    // Else, send election message to all participants with higher id
    Election::send_election(node);
    // Wait for answers
    msleep(TIMEOUT_ELECTION);
    if (Election::check_reply_from_election(node)) {
        msleep(TIMEOUT_COORDINATOR); // waits for coordinator message, if timeout, starts new election
        int coordinator_id = Election::check_coordinator(node);
        if (coordinator_id == -1) {
            node.has_started_election = false;
            goto restart_election;
            //Election::run_election(node); // might break the universe
        }
        else {
            node.has_started_election = false;
            node.manager_id = coordinator_id; // updates new coordinator
        }
        return;
    }
    // If no answer, send coordinator message
    Election::send_coordinator(node);
    node.manager_id = node.info.id;
    node.has_started_election = false;
}

#endif ELECTION_IMPLEMENTATION
