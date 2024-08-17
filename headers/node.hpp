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
#include "macros.h"
#include "Net/Socket.hpp"
#include "discovery_service.h"

#define INITIAL_ID 1000
#define CLEAR_SCREEN "\033[2J" // ascii escape code to clear the screen

struct Message {
    int id;
    char msg;
};

class Node {
    public:
        participant_t info = {}; // info about this node
        ParticipantTable participants = {}; // every node keeps a copy of the participants table
        int manager_id = -1; // id of the manager node
        bool has_started_election = false;
        DiscoveryService ds = {};

        Node(bool is_server);
        ~Node();

        void run_node();
        void end_node();
        bool is_manager();
        bool my_self(participant_t &participant);
        int last_id();
};

#endif // NODE_H_

#ifndef NODE_IMPLEMENTATION
#define NODE_IMPLEMENTATION

Node::Node(bool is_server) {
    this->ds.port = INITIAL_PORT + 0;
    if (is_server) {
        this->info.id = INITIAL_ID;
        this->manager_id = this->info.id;
    }    
}

Node::~Node() {

}

void Node::run_node() {
    StringEqComparerIgnoreCase string_equals;
    if (this->is_manager()) {
        this->ds.start_server();
        //monitoring_service.start_server(participants);

        help_msg_server();
        this->participants.print();

        while (1) {
            this->participants.lock();   
            if (key_hit()) {
                command_exec(participants);
            }
            
            if (this->participants.dirty) {
                std::cout << CLEAR_SCREEN << "Manager\n";
                help_msg_server();
                this->participants.print();
            }

            MachineEndpoint discoveredMachine;
            if (this->ds.endpoints.dequeue(discoveredMachine)) {
                this->participants.add(participant_t{
                    .machine = discoveredMachine,
                    .status = true,
                    .socket = std::make_shared<Socket>(),
                    .last_conection_timestamp = time(NULL)});
            }

            this->participants.unlock();
            msleep(300); // Let other threads get the GODDAMN MUTEX
        }
    } else {
        NetworkInterfaceList network_interfaces = NetworkInterfaceList::begin();
        std::cout << "MAC ADDRESS: " << MacAddress::get_mac().mac_str << "\nHOSTNAME: " << get_hostname() << "\n"
                    << network_interfaces->to_string() << std::endl;
        help_msg_client();
        this->ds.start_client();

        while (1) {
            if (key_hit()) {
                string cmd;
                std::cin >> cmd;
                if (string_equals(cmd, "EXIT")) {
                    //monitoring_service.tcp_socket.send("exit");
                    exit(EXIT_SUCCESS);
                }
            }
            MachineEndpoint server_machine_endpoint;
            // if (!monitoring_service.running && discovery_service.endpoints.dequeue(server_machine_endpoint)) {
            //     monitoring_service.start_client(server_machine_endpoint);
            // }
        }
    }
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

bool Node::is_manager() {
    return this->manager_id == this->info.id;
}   

bool Node::my_self(participant_t &participant) {
    return participant.id == this->info.id;
}

#endif // NODE_IMPLEMENTATION