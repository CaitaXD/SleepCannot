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
        std::mutex msg_mutex; // only one thread can access messages at a time
        std::vector<Message> messages; // messages received by this node

        Node(int id, int manager_id);
        ~Node();
        void start_node(); // this function should connect node to all other nodes
        void end_node();
        void listen(); // listen for messages, updating message vector  
};

#endif // NODE_H_

#ifndef NODE_IMPLEMENTATION
#define NODE_IMPLEMENTATION

Node::Node(int id, int manager_id) {

}

Node::~Node() {
    
}

void Node::start_node() {

}

void Node::end_node() {

}

// run in thread (TODO: implement on monitoring maybe)
void Node::listen() {
    this->msg_mutex.lock();
    this->messages.clear();
    this->msg_mutex.unlock();
    while (true) {
        std::string msg;
        this->info.socket->recv(&msg); // check if it is blocking
        char msg_type = msg[0]; // check if message is of type 'e', 'c' or 'a'
        int id = std::stoi(msg.substr(2));
        this->msg_mutex.lock();
        this->messages.push_back(Message{.id = id, .msg = msg_type});
        this->msg_mutex.unlock();
        msleep(300); // let other threads get the GODDAMN mutex
    }
}

#endif // NODE_IMPLEMENTATION