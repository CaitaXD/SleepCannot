/*
    Represents a node participating on the service.
    The node can be a manager or a client (also a potential manager).
    The ideia is to make the entire system in the form of a graph.
*/

#ifndef NODE_H_
#define NODE_H_

#include <iostream>
#include <vector>
#include <mutex>
#include <string>
#include "management.hpp"
#include "Net/Socket.hpp"
#include "macros.h"

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
*/

struct Message {
    int id;
    char msg;
};

class Node {
    participant_t info; // info about this node
    ParticipantTable participants; // every node keeps a copy of the participants table
    int manager_id = -1; // id of the manager node
    bool has_started_election = false;
    std::mutex msg_mutex; // only one thread can access messages at a time
    std::vector<Message> messages; // messages received by this node

    Node(int id, int manager_id);
    ~Node();
    void start_node(); // this function should connect node to all other nodes
    void end_node();

    bool should_run_election(); // determined in monitoring service
    void run_election();
    void send_election();
    void answer_election();
    void send_coordinator();
    
    void listen(); // listen for messages, updating message vector
    bool check_reply_from_election(); // check if there is a reply from election message
    int check_coordinator(); // check if there is a coordinator message and returns its id (or -1 if no coordinator)
};

#endif // NODE_H_

#ifndef NODE_IMPLEMENTATION
#define NODE_IMPLEMENTATION

// run in thread
void Node::listen() {
    this->msg_mutex.lock();
    this->messages.clear();
    this->msg_mutex.unlock();
    while (true) {
        std::string msg;
        this->info.socket->recv(&msg);
        char msg_type = msg[0];
        int id = std::stoi(msg.substr(2));
        this->msg_mutex.lock();
        this->messages.push_back(Message{.id = id, .msg = msg_type});
        this->msg_mutex.unlock();
        msleep(300); // let other threads get the GODDAMN mutex
    }
}

void Node::send_coordinator() {
    this->participants.lock();
    // send coordinator message to all participants
    for (auto &[host, participant] : participants.map) {
        participant.socket->send("c," + std::to_string(this->info.id));
    }
    this->participants.unlock();
}

void Node::send_election() {
    if (this->has_started_election) return; // only one election message per node TODO: no busy waiting
    this->has_started_election = true;
    // send election message to participants with higher id
    this->participants.lock();
    for (auto &[host, participant] : participants.map) {
        if (this->info.id < participant.is_manager) {
            participant.socket->send("e," + std::to_string(this->info.id));
        }
    }
    this->participants.unlock();
}

// run in thread (maybe on listen)
void Node::answer_election() {
    this->msg_mutex.lock();
    if (this->messages.size() == 0) {
        this->msg_mutex.unlock();
        return;
    }
    for (auto it = this->messages.begin(); it != this->messages.end(); ++it) {
        auto &msg = *it;
        if (msg.msg == 'e') {
            int dest_id = msg.id;
            if (this->info.id < dest_id) {
                this->participants.lock();
                for (auto &[host, participant] : participants.map) {
                    if (participant.id == dest_id) {
                        participant.socket->send("a," + std::to_string(this->info.id));
                        if (!this->has_started_election) this->run_election();
                        this->messages.erase(it);
                        this->participants.unlock();
                        this->participants.unlock();
                        return;
                    }
                }
                this->participants.unlock();
            }
        }
    }
    this->participants.unlock();
}

// run in thread (maybe on listen)
int Node::check_coordinator() {
    this->msg_mutex.lock();
    if (this->messages.size() == 0) {
        this->msg_mutex.unlock();
        return false;
    }
    for (auto it = this->messages.begin(); it != this->messages.end(); ++it) {
        auto &msg = *it;
        if (msg.msg == 'c') {
            this->msg_mutex.unlock();
            this->messages.erase(it);
            return msg.id;
        }
    }
    this->msg_mutex.unlock();
    return -1;
}

bool Node::check_reply_from_election() {
    this->msg_mutex.lock();
    if (this->messages.size() == 0) {
        this->msg_mutex.unlock();
        return false;
    }
    for (auto it = this->messages.begin(); it != this->messages.end(); ++it) {
        auto &msg = *it;
        if (msg.msg == 'a') {
            if (this->info.id < msg.id) {
                this->msg_mutex.unlock();
                this->messages.erase(it);
                return true;
            }
        }
    }
    this->msg_mutex.unlock();
    return false;
}

// run in thread
void Node::run_election() {
    // if (!this->should_run_election()) return;
    // sends coordinator message if it has the highest id
    bool highest_id = true;
    this->participants.lock();
    for (auto &[host, participant] : participants.map) {
        if (!participant.status) continue;
        if (this->info.id < participant.id) {
            highest_id = false;
            break;
        }
    }
    this->participants.unlock();
    if (highest_id) {
        this->send_coordinator();
        this->has_started_election = false;
        return;
    }
    // else, send election message to all participants with higher id
    this->send_election();
    // wait for answers
    msleep(TIMEOUT_ELECTION);
    if (this->check_reply_from_election()) {
        msleep(TIMEOUT_COORDINATOR); // waits for coordinator message, if timeout, starts election
        int coordinator_id = this->check_coordinator();
        if (coordinator_id == -1) {
            this->has_started_election = false;
            this->run_election();
        }
        else {
            this->has_started_election = false;
            this->manager_id = coordinator_id;
            return;
        }
    }
    // if no answer, send coordinator message
    this->send_coordinator();
    this->has_started_election = false;
    return;
}

#endif // NODE_IMPLEMENTATION