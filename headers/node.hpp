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
#include <pthread.h>
#include "management.hpp"
#include "Net/Socket.hpp"
#include "macros.h"

#include "DataStructures/LockFreeQueue.h"

#define INITIAL_ID 1000

struct Message {
    int id;
    char msg;
};

class Node {
    public:
        participant_t info; // info about this node
        ParticipantTable participants; // every node keeps a copy of the participants table
        int manager_id = -1; // id of the manager node
        bool has_started_election = false;
        pthread_t accept_thread = {};
        // std::mutex msg_mutex; // only one thread can access messages at a time
        // std::vector<Message> messages; // messages received by this node

        Node(participant_t info, int manager_id);
        Node(participant_t info);
        ~Node();
        void start_node(Concurrent::LockFreeQueue<MachineEndpoint> &queue); // this function should connect node to all other nodes
        void end_node();
        // void listen(); // listen for messages, updating message vector
        void accept_nodes(); // accept connections from other nodes
        bool is_manager();
        bool my_self(participant_t &participant);
        int last_id();
};

#endif // NODE_H_

#ifndef NODE_IMPLEMENTATION
#define NODE_IMPLEMENTATION

Node::Node(participant_t info, int manager_id) {
    this->info = info;
    this->manager_id = manager_id;
}

Node::Node(participant_t info) {
    this->info = info;
    this->manager_id = info.id;
}

void Node::start_node(Concurrent::LockFreeQueue<MachineEndpoint> &queue) {
    participants.lock();
    participants.add(info);
    participants.unlock();

    while (true) {
        if (is_manager()) {
            MachineEndpoint discoveredMachine;
            if (queue.dequeue(discoveredMachine)) {
                participants.lock();
                
                if (participants.map.find(discoveredMachine.hostname) != participants.map.end()) {
                    participants.unlock();
                    continue;
                }
                
                participants.add(participant_t{
                    .machine = discoveredMachine,
                    .status = true,
                    .socket = std::make_shared<Socket>(),
                    .last_conection_timestamp = time(NULL)
                });
                
                participants.unlock();
            }   
        }
    }
}

void Node::end_node() {

}

int Node::last_id() {
    this->participants.lock();
    int last_id = INITIAL_ID;
    for (auto &[host, participant] : this->participants.map) {
        if (participant.id < last_id) last_id = participant.id;
    }
    this->participants.unlock();

    return last_id;
}

// // run in thread (TODO: implement on monitoring maybe)
// void Node::listen() {
//     this->msg_mutex.lock();
//     this->messages.clear();
//     this->msg_mutex.unlock();
//     while (true) {
//         std::string msg;
//         this->info.socket->recv(&msg); // check if it is blocking
//         char msg_type = msg[0]; // check if message is of type 'e', 'c' or 'a'
//         int id = std::stoi(msg.substr(2));
//         this->msg_mutex.lock();
//         this->messages.push_back(Message{.id = id, .msg = msg_type});
//         this->msg_mutex.unlock();
//         msleep(300); // let other threads get the GODDAMN mutex
//     }
// }

bool Node::is_manager() {
    return this->manager_id == this->info.id;
}   

bool Node::my_self(participant_t &participant) {
    return participant.id == this->info.id;
}

#endif // NODE_IMPLEMENTATION