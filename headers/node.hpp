/*
    Represents a node participating on the service.
    The node can be a manager or a client (also a potential manager).
    The ideia is to make the entire system in the form of a graph.
*/

#ifndef NODE_H_
#define NODE_H_

#define INITIAL_ID 1000
#define CLEAR_SCREEN "\033[2J"   // ascii escape code to clear the screen
#define TIMEOUT_ELECTION 2500    // ms
#define TIMEOUT_COORDINATOR 5000 // ms

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
    bool election_answered = false;
    bool received_coordinator = false;
    DiscoveryService ds = {};
    MonitoringService *ms = monitoring_service(this);

    Node(bool is_server);
    ~Node();

    void run_node();
    void fill_table();
    void end_node();
    bool is_manager();
    void change_manager(int new_manager_id);
    bool my_self(participant_t &participant);
    bool my_fd(int fd);
    int last_id();
    void start_serve_peers(int backlog = 5);
    Socket connect_peer(MachineEndpoint &peer_endpoint);
    std::unordered_map<string, std::tuple<MachineEndpoint, Socket>> connect_to_peers(std::vector<MachineEndpoint> &endpoints);

    // Election
    void run_election();  // starts election process
    void send_election(); // sends election message to all participants with higher id
    void handle_election_response(string &buffer);
    void answer_election(int sender_id); // answers election message
    void send_coordinator();             // sends coordinator message to all participants
    // TODO: implement on monitoring maybe
    bool should_run_election();       // determined in monitoring service
    bool check_reply_from_election(); // check if there is a reply from election message
    int check_coordinator();          // check if there is a coordinator message and returns its id (or -1 if no coordinator)

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

void Node::change_manager(int new_manager_id)
{
    if (is_manager())
    {
        ds.stop();
        info.is_manager = false;
        ds.start_client();
    }

    if (new_manager_id == info.id)
    {
        ds.stop();
        info.is_manager = true;
        ds.start_server();
    }

    manager_id = new_manager_id;
}

void Node::run_node()
{
    StringEqComparerIgnoreCase string_equals;
    start_serve_peers();
    monitoring_service_start(ms);
restart:
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
    }
    rsleep();
    goto restart;
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
    wait:
        auto optional_participant = participants.find_by_address(peer_endpoint);
        if (!optional_participant.has_value())
        {
            goto wait;
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

                wait:
                auto optional_peer = participants.find_by_address(peerAddress);
                if (!optional_peer.has_value()) {
                    goto wait;
                    //participants.unlock(); 
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
        Socket socket = connect_peer(peer_endpoint);
        if (socket.file_descriptor == -1)
        {
            continue;
        }
        sockets.emplace(peer_endpoint.hostname, std::make_tuple(peer_endpoint, std::move(socket)));
    }
    return sockets;
}

Socket Node::connect_peer(MachineEndpoint &peer_endpoint)
{
    if (peer_endpoint == info.machine)
        return Socket{};

    Socket socket{};
    int result = socket.open(SocketType::Stream, SocketProtocol::TCP);
    result |= socket.set_option(SO_REUSEADDR, 1);
    if (result < 0)
    {
        perrorcode("Node::connect_to_peers");
        return Socket{};
    }
try_connect:
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
        return Socket{};
    }
    return socket;
}

bool Node::my_fd(int fd)
{
    return info.socket->file_descriptor == fd;
}

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

string padleft(const string &str, int len, char c = '0')
{
    return str + string(len - str.length(), c);
}

// Send coordinator message to all participants
void Node::send_coordinator()
{
    participants.lock();
    for (auto &[host, participant] : participants.map)
    {
        LOGF("Sending coordinator message to %s", host.c_str());
        int read = participant.socket->send("Ec" + padleft(std::to_string(info.id), 3));
        if (read < 0)
        {
            perrorcode("send");
        }
        if (read == 0)
        {
            LOG("No one is listening");
        }
    }
    participants.unlock();
}

// Send election message to participants with higher id
void Node::send_election()
{
    election_answered = false;
    participants.lock();
    std::vector<FileDescriptor *> fds;
    for (auto &[host, participant] : participants.map)
    {
        if (info.id < participant.id)
        {
            LOGF("Sending election message to %s", host.c_str());
            int read = participant.socket->send("Ee" + padleft(std::to_string(info.id), 3));
            if (read < 0)
            {
                perrorcode("send");
                continue;
            }
            else
            {
                fds.push_back(participant.socket.get());
            }
        }
    }
    std::vector<pollfd> poll_result = FileDescriptor::poll(fds, POLLIN, 5000);
    LOGF("Poll result size %zu", poll_result.size());
    for (auto &poll : poll_result)
    {
        Socket sock = Socket(poll.fd);
        sock.keep_alive = true;
        string buffer(1024, '\0');
        int read = sock.recv(&buffer);
        if (read < 0)
        {
            perrorcode("recv");
            continue;
        }
        handle_election_response(buffer);
    }

    participants.unlock();
}

void Node::handle_election_response(string &buffer)
{
    // Eleciton
    if (buffer[0] == 'E' && (buffer[1] == 'a' || buffer[1] == 'c' || buffer[1] == 'e'))
    {
        LOGF("Election message received %s", buffer.c_str());
        char type = buffer[1];
        int id = std::stoi(buffer.substr(2, 3).c_str());
        switch (type)
        {
        case 'c':
            received_coordinator = true;
            change_manager(id);
            break;
        case 'e':
            answer_election(id);
            break;
        case 'a':
            election_answered = true;
            break;
        default:
            break;
        }
    }
    else
    {
        LOGF("Received %s instead of election message", buffer.c_str());
    }
}

// run in thread (maybe on listen, which might be on monitoring service)
// Answers election message of lower id node and starts election if it has not started
void Node::answer_election(int sender_id)
{
    if (info.id > sender_id)
    {
        participants.lock();
        for (auto &[host, participant] : participants.map)
        {
            if (participant.id != sender_id)
                continue;
            LOGF("Answering election message from %s", host.c_str());
            int read = participant.socket->send("Ea" + padleft(std::to_string(info.id), 3));
            if (read < 0)
            {
                perrorcode("send");
                continue;
            }
            if (!has_started_election)
                run_election();
            participants.unlock();
            return;
        }
        participants.unlock();
    }
}

int Node::check_coordinator()
{
    return 0;
}

bool Node::check_reply_from_election()
{
    return true;
}

// run in thread
// Starts election process
void Node::run_election()
{
    LOG("Election started");
restart_election:
    if (has_started_election)
        return;
    has_started_election = true;
    // Sends coordinator message if it has the highest id
    bool highest_id = true;
    participants.lock();
    for (auto &[host, participant] : participants.map)
    {
        if (!participant.status)
            continue;
        if (info.id < participant.id)
        {
            highest_id = false;
            break;
        }
    }
    participants.unlock();
    if (highest_id)
    {
        LOG("Ready to mingle");
        send_coordinator();
        change_manager(info.id);
        has_started_election = false;
        return;
    }
    // Else, send election message to all participants with higher id
    send_election();
    // Wait for answers
    msleep(TIMEOUT_ELECTION);
    if (election_answered)
    {
        msleep(TIMEOUT_COORDINATOR); // waits for coordinator message, if timeout, starts new election
        if (!received_coordinator)
        {
            has_started_election = false;
            goto restart_election;
            // run_election(); // might break the universe
        }
        else
        {
            has_started_election = false;
        }
        return;
    }
    // If no answer, send coordinator message
    LOGF("Everyone is dead so i must be the boss");
    send_coordinator();
    change_manager(info.id);
    has_started_election = false;
    LOG("Election finished");
}

#endif // NODE_IMPLEMENTATION