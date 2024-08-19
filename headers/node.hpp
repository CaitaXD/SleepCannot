/*
    Represents a node participating on the service.
    The node can be a manager or a client (also a potential manager).
    The ideia is to make the entire system in the form of a graph.
*/

#ifndef NODE_H_
#define NODE_H_

// forward declarations CIRCULAR REFERENCES ARE PAINFUL
class MonitoringService;
class MonitoringService *monitoring_service(class Node *node);
void monitoring_service_start(class MonitoringService *ms);

#include <iostream>
#include <vector>
#include <mutex>
#include <string>
#include <pthread.h>
#include "management.hpp"
#include "macros.h"
#include "Net/Socket.hpp"
#include "discovery_service.h"
#include "monitoring_service.h"

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
    MonitoringService *ms = monitoring_service(this);

    Node(bool is_server);
    ~Node();

    void run_node();
    void fill_table();
    void end_node();
    bool is_manager();
    bool my_self(participant_t &participant);
    bool my_fd(int fd);
    int last_id();
    void start_serve_peers(int backlog = 5);
    std::unordered_map<string, std::tuple<MachineEndpoint, Socket>> connect_to_peers(std::vector<MachineEndpoint> &endpoints);

private:
    pthread_t serve_peers_thread = {};
};

void node_connect_to_peers(Node *node);

#endif // NODE_H_

#ifndef NODE_IMPLEMENTATION
#define NODE_IMPLEMENTATION

std::unordered_map<string, std::tuple<MachineEndpoint, Socket>> node_connect_to_peers(Node *node, std::vector<MachineEndpoint> &endpoints)
{
    return node->connect_to_peers(endpoints);
}

Node::Node(bool is_server)
{
    ds.port = DISCOVERY_PORT;
    if (is_server)
    {
        info.id = INITIAL_ID;
        manager_id = info.id;
    }
    info.socket = std::make_shared<Socket>();
    info.is_manager = is_server;
    info.status = true;
    info.last_conection_timestamp = time(NULL);
    info.machine = MachineEndpoint::MyMachine(AddressFamily::InterNetwork, TCP_PORT);
}

Node::~Node()
{
    pthread_join(this->serve_peers_thread, NULL);
    free(this->ms);
}

// void Node::ms_start(class MonitoringService *ms) {
//     ms->start_service();
// }

void Node::run_node()
{
    StringEqComparerIgnoreCase string_equals;
    start_serve_peers();
    monitoring_service_start(ms);
    if (is_manager())
    {
        ds.start_server();

        help_msg_server();
        participants.print();

        while (is_manager())
        {
            // participants.lock();
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

            fill_table();

            // participants.unlock();
            rsleep(); // Let other threads get the GODDAMN MUTEX
        }
        ds.stop();
        // ms->stop();
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
                    info.socket->send("exit");
                    exit(EXIT_SUCCESS);
                }
            }

            fill_table();
        }
        ds.stop();
        // ms->stop();
    }
}

void Node::fill_table()
{
    std::vector<MachineEndpoint> discoveredMachines;
    MachineEndpoint discoveredMachine;

    // participants.lock();
    {
        while (ds.endpoints.dequeue(discoveredMachine))
        {
            LOGF("Discovered machine: %s", discoveredMachine.to_string().c_str());
            discoveredMachines.push_back(discoveredMachine);
            auto map = participants.map;
            if (map.find(discoveredMachine.hostname) != map.end())
                continue;

            participant_t participant = participant_t{
                .machine = discoveredMachine,
                .status = true,
                .socket = std::make_shared<Socket>(Socket{}),
                .last_conection_timestamp = time(NULL),
                .id = is_manager() ? (last_id() - 1) : (-1),
                .is_manager = is_manager()};

            participants.add(participant);
        }
    }
    // participants.unlock();

    auto socks = connect_to_peers(discoveredMachines);
    for (auto &[hostname, tuple] : socks)
    {

        auto &[peer_endpoint, socket] = tuple;
        auto optional_participant = participants.find_by_address(peer_endpoint);

        if (!optional_participant.has_value())
        {
            std::eprintf("How did we get here?");
            continue;
        }

        auto &[perr_name, peer] = optional_participant.value();
        *peer.get().socket = std::move(socket);
    }
}

int Node::last_id()
{
    int last_id = INITIAL_ID;
    for (auto &[host, participant] : this->participants.map)
    {
        if (participant.id < last_id)
            last_id = participant.id;
    }
    return last_id;
}

bool Node::is_manager()
{
    return manager_id == info.id;
}

bool Node::my_self(participant_t &participant)
{
    return participant.id == this->info.id;
}

void Node::start_serve_peers(int backlog)
{
    if (info.socket == nullptr)
    {
        info.socket = std::make_shared<Socket>();
    }

    Socket &socket = *info.socket;
    int result = 0;
    if (socket.file_descriptor == -1)
    {
        result |= socket.open(SocketType::Stream, SocketProtocol::TCP);
        result |= socket.set_option(SO_REUSEADDR, 1);
        result |= socket.bind(TCP_PORT);
        result |= socket.listen(backlog);
        LOGF("Listening on port %d", TCP_PORT);
    }

    if (result < 0)
    {
        perrorcode("Node::listen");
        return;
    }

    pthread_create(&serve_peers_thread, NULL, [](void *data) -> void *
                   {
        LOGF("Node::serve_peers_thread started");
        Node *node = (Node *)data;
        ParticipantTable &participants = node->participants;
        Socket& server_socket = *node->info.socket;

        while(true) {
            //int result = 0;
            //participants.lock();
            {
                IpEndpoint peerAddress;
                LOGF("Waiting for connection");
                Socket client_socket = server_socket.accept(peerAddress);
                LOGF("Connection from %s", peerAddress.to_string().c_str());
                if (client_socket.lasterrno != 0) {
                    perrorcode("Node::accept");
                    participants.unlock();
                    continue;
                }

                auto optional_peer = participants.find_by_address(peerAddress.with_port(TCP_PORT));
                if (!optional_peer.has_value()) {
                    std::eprintf("How did we get here?");
                    participants.unlock(); 
                    continue;
                }

                auto &[perr_name, peer] = optional_peer.value();
                *peer.get().socket = std::move(client_socket);
            }
            //participants.unlock();
            rsleep(); // Let other threads get the GODDAMN MUTEX
        }

        return NULL; }, this);
}

std::unordered_map<string, std::tuple<MachineEndpoint, Socket>> Node::connect_to_peers(std::vector<MachineEndpoint> &endpoints)
{
    std::unordered_map<string, std::tuple<MachineEndpoint, Socket>> sockets;
    for (auto &peer_endpoint : endpoints)
    {
        Socket socket = Socket();
        int result = socket.open(SocketType::Stream, SocketProtocol::TCP);
        result |= socket.set_option(SO_REUSEADDR, 1);
        if (result < 0)
        {
            perrorcode("Node::connect_to_peers");
            continue;
        }
    try_connect:
        LOGF("Attempting to connect to %s", peer_endpoint.to_string().c_str());
        result = socket.connect(peer_endpoint.with_port(TCP_PORT));
        if (result < 0)
        {
            if (socket.lasterrno == ECONNREFUSED)
            {
                LOGF("Connection refused");
                rsleep();
                goto try_connect;
            }
            perrorcode("Node::connect_to_peers");
            continue;
        }
        else
        {
            LOGF("Connected to %s", peer_endpoint.to_string().c_str());
        }
        sockets.emplace(peer_endpoint.hostname, std::make_tuple(peer_endpoint, std::move(socket)));
    }
    if (sockets.size() > 0)
    {
        LOGF("Connected to %zu peers", sockets.size());
    }
    return sockets;
}

bool Node::my_fd(int fd)
{
    return info.socket->file_descriptor == fd;
}

#endif // NODE_IMPLEMENTATION