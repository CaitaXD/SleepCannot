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

struct Message
{
    int id;
    char msg;
};

class Node
{
public:
    participant_t info = {};            // info about this node
    ParticipantTable participants = {}; // every node keeps a copy of the participants table
    int manager_id = -1;                // id of the manager node
    bool has_started_election = false;
    DiscoveryService ds = {};
    MonitoringService ms = {*this};

    Node(bool is_server);
    ~Node();

    void run_node();
    void end_node();
    bool is_manager();
    bool my_self(participant_t &participant);
    int last_id();

private:
    pthread_t serve_peers_thread = {};

    void start_serve_peers(int backlog = 5);
    void connect_to_peers();
};

#endif // NODE_H_

#ifndef NODE_IMPLEMENTATION
#define NODE_IMPLEMENTATION

Node::Node(bool is_server)
{
    ds.port = INITIAL_PORT + 0;
    if (is_server)
    {
        info.id = INITIAL_ID;
        manager_id = info.id;
    }
}

Node::~Node()
{
}

void Node::run_node()
{
    StringEqComparerIgnoreCase string_equals;
    start_serve_peers();
    if (is_manager())
    {
        ds.start_server();
        // ms.start_server(participants);

        help_msg_server();
        participants.print();

        while (is_manager())
        {
            participants.lock();
            if (key_hit())
            {
                command_exec(participants);
            }

            if (participants.dirty)
            {
                std::cout << CLEAR_SCREEN << "Manager\n";
                help_msg_server();
                participants.print();
            }

            MachineEndpoint discoveredMachine;
            if (ds.endpoints.dequeue(discoveredMachine))
            {
                auto map = participants.map;
                if (map.find(discoveredMachine.hostname) != map.end())
                    continue;

                participants.add(participant_t{
                    .machine = discoveredMachine,
                    .status = true,
                    .socket = std::make_shared<Socket>(),
                    .last_conection_timestamp = time(NULL),
                    .id = last_id() - 1});
            }

            participants.unlock();
            msleep(300); // Let other threads get the GODDAMN MUTEX
        }
        ds.stop();
    }
    else
    {
        NetworkInterfaceList network_interfaces = NetworkInterfaceList::begin();
        std::printf("MAC ADDRESS: %s\nHOSTNAME: %s\n%s\n", MacAddress::get_mac().mac_str, get_hostname().c_str(), network_interfaces->to_string().c_str());

        help_msg_client();
        ds.start_client();

        while (!is_manager())
        {
            if (key_hit())
            {
                string cmd;
                std::cin >> cmd;
                if (string_equals(cmd, "EXIT"))
                {
                    // ms.tcp_socket.send("exit");
                    exit(EXIT_SUCCESS);
                }
            }
            MachineEndpoint server_machine_endpoint;
            // if (!ms.running && discovery_service.endpoints.dequeue(server_machine_endpoint)) {
            //     ms.start_client(server_machine_endpoint);
            // }
        }
    }
}

int Node::last_id()
{
    this->participants.lock();
    int last_id = INITIAL_ID;
    for (auto &[host, participant] : this->participants.map)
    {
        if (participant.id < last_id)
            last_id = participant.id;
    }
    this->participants.unlock();
    return last_id;
}

bool Node::is_manager()
{
    return this->manager_id == this->info.id;
}

bool Node::my_self(participant_t &participant)
{
    return participant.id == this->info.id;
}

void Node::start_serve_peers(int backlog)
{
    Socket &socket = *info.socket;
    int result = 0;
    if (socket.file_descriptor == -1)
    {
        result |= socket.open(SocketType::Stream, SocketProtocol::TCP);
        result |= socket.bind(info.machine.get_port());
        result |= socket.listen(backlog);
        result |= socket.set_option(SO_REUSEADDR, 1);
    }
   

    if (result < 0)
    {
        perrorcode("Node::listen");
        return;
    }

    pthread_create(&serve_peers_thread, NULL, [](void *data) -> void *
                   {
        Node *node = (Node *)data;
        ParticipantTable &participants = node->participants;
        Socket& server_socket = *node->info.socket;

        while(true) {
            int result = 0;
            participants.lock();
            {
                if (participants.map.size() == 0) {
                    participants.unlock();
                    continue;
                }

                MachineEndpoint peerAddress;
                Socket client_socket = server_socket.accept(peerAddress);
                if (client_socket.lasterrno != 0) {
                    perrorcode("Node::accept");
                    participants.unlock();
                    continue;
                }

                auto optional_peer = participants.find_by_address(peerAddress);
                if (!optional_peer.has_value()) {
                    std::eprintf("How did we get here?");
                    participants.unlock(); 
                    continue;
                }

                auto &[perr_name, peer] = optional_peer.value();
                *peer.get().socket = std::move(client_socket);
            }
            participants.unlock();
            msleep(300); // Let other threads get the GODDAMN MUTEX
        }

        return NULL; }, this);
}

void Node::connect_to_peers()
{
    participants.lock();
    for (auto &[host, participant] : participants.map)
    {
        if (participant.socket->file_descriptor > -1)
            continue;

        Socket &socket = *participant.socket;
        int result = socket.open(SocketType::Stream, SocketProtocol::TCP);
        result |= socket.set_option(SO_REUSEADDR, 1);
        if (result < 0)
        {
            perrorcode("Node::connect_to_peers");
            continue;
        }
    try_connect:
        result = socket.connect(participant.machine);
        if (result < 0)
        {
            perrorcode("Node::connect_to_peers");
            if (socket.lasterrno == ECONNREFUSED)
            {
                msleep(300);
                goto try_connect;
            }
            continue;
        }
    }
    participants.unlock();
    msleep(300); // Let other threads get the GODDAMN MUTEX
}

#endif // NODE_IMPLEMENTATION