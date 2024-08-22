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
#include <condition_variable>

#define INITIAL_ID 1000
#define CLEAR_SCREEN "\033[2J" // ascii escape code to clear the screen

struct Message
{
    int id;
    char msg;
};

enum ElectionState
{
    NoElection,
    Running,
    Overruled,
};

class Node
{
public:
    participant_t info = {};            // info about this node
    ParticipantTable participants = {}; // every node keeps a copy of the participants table
    int manager_id = -1;                // id of the manager node
    bool election_running = false;
    bool election_answered = false;
    bool received_coordinator = false;
    ElectionState election_state = NoElection;
    int highest_id_in_election = -1;
    time_t election_start_time = 0;
    DiscoveryService ds = {};
    MonitoringService *ms = monitoring_service(this);

    Node(bool is_server);
    ~Node();

    void run_node();
    void discovery();
    void end_node();
    bool is_manager();
    void change_manager(int new_manager_id);
    bool my_self(participant_t &participant);
    bool my_fd(int fd);
    int last_id();
    void start_serve_peers(int backlog = 5);
    int connect_peer(participant_t &peer);
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
    bool has_election_message(string &buffer);

private:
    pthread_t serve_peers_thread = {};
};

void node_connect_to_peers(Node *node);

#endif // NODE_H_

#ifndef NODE_IMPLEMENTATION
#define NODE_IMPLEMENTATION

Node::Node(bool is_server)
{
    ds.port = DISCOVERY_PORT;
    info.socket = std::make_shared<Socket>();
    info.is_manager = is_server;
    info.status = true;
    info.last_conection_timestamp = time(NULL);
    info.machine = MachineEndpoint::MyMachine(InternetAddress::Loopback, TCP_SERVER_PORT);

    LOGF("My machine: %s", info.machine.to_string().c_str());
    if (is_server)
    {
        info.id = INITIAL_ID;
        manager_id = info.id;
        participants.add(info);
    }
}

Node::~Node()
{
    pthread_join(this->serve_peers_thread, NULL);
    free(this->ms);
}

void Node::change_manager(int new_manager_id)
{
    auto &old_manager = participants.find_by_id_blocking(manager_id);
    old_manager.is_manager = false;

    auto &new_manager = participants.find_by_id_blocking(new_manager_id);
    new_manager.is_manager = true;

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

    participants.dirty = true;
    manager_id = new_manager_id;
}

void Node::run_node()
{
    StringEqComparerIgnoreCase string_equals;
    start_serve_peers(6);
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
                // std::cout << CLEAR_SCREEN << "Manager\n";
                help_msg_server();
                participants.print();
            }

            discovery();
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

            discovery();
        }
    }
    rsleep();
    goto restart;
}

void Node::discovery()
{
    MachineEndpoint discoveredMachine;
    while (ds.endpoints.dequeue(discoveredMachine))
    {
        auto map = participants.map;
        if (map.find(discoveredMachine.hostname) != map.end())
            continue;

        participant_t participant = participant_t{
            .machine = discoveredMachine,
            .status = true,
            .socket = std::make_shared<Socket>(),
            .last_conection_timestamp = time(NULL),
            .id = is_manager() ? (last_id() - 1) : INITIAL_ID,
            .is_manager = is_manager() == false};

        LOGF("Discovered machine: %s id %d", discoveredMachine.to_string().c_str(), participant.id);
        participants.add(participant);

        if (!is_manager())
        {
            int result = connect_peer(participant);
            if (result < 0)
            {
                perrorcode("Node::discovery");
                exit(EXIT_FAILURE);
            }
        }
    }
}

int Node::last_id()
{
    int last_id = INITIAL_ID;
    for (auto &[host, participant] : participants.map)
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

    Socket &server_socket = *info.socket;
    int result = 0;
    if (server_socket.file_descriptor == -1)
    {
        result |= server_socket.open(SocketType::Stream, SocketProtocol::TCP);
        result |= server_socket.set_option(SO_REUSEADDR, 1);
        result |= server_socket.bind(TCP_SERVER_PORT);
        result |= server_socket.listen(backlog);
        LOGF("Listening on port %d", TCP_SERVER_PORT);
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
            if (participants.lock_if([](ParticipantTable &p) -> bool { return p.map.size() > 1; }))
            {
                IpEndpoint peerAddress = {};
                Socket peer_client_socket = server_socket.accept(peerAddress);
                if (peer_client_socket.file_descriptor < 0) {
                    peer_client_socket.lasterrno = 0;
                    perrorcode("Node::accept");
                    participants.unlock();
                    continue;
                }
                LOGF("Accepted Connection from %s", peerAddress.to_string().c_str());
                auto &peer = participants.find_by_address_blocking(peerAddress);
                *peer.socket = std::move(peer_client_socket);
                participants.unlock();
            }
            rsleep(); // Let other threads get the GODDAMN MUTEX
        }

        return NULL; }, this);
}

int Node::connect_peer(participant_t &peer)
{
    if (peer.machine == info.machine)
        return -1;

    Socket client_socket;
    int result = client_socket.open(SocketType::Stream, SocketProtocol::TCP);
    result |= client_socket.set_option(SO_REUSEADDR, 1);
    result |= client_socket.connect(peer.machine.with_port(TCP_SERVER_PORT));

    if (result < 0)
        return result;

    LOGF("Connected to %s", peer.machine.with_port(TCP_SERVER_PORT).to_string().c_str());

    *peer.socket = std::move(client_socket);
    return 0;
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
    // participants.lock();
    for (auto &[host, participant] : participants.map)
    {
        if (info.id == participant.id)
            continue;
        LOGF("Sending coordinator message to %s id %d", participant.machine.to_string().c_str(), participant.id);
        int read = participant.socket->send("Ec" + padleft(std::to_string(info.id), 3));
        if (read < 0)
        {
            perrorcode("send");
        }
    }
    // participants.unlock();
}

// Send election message to participants with higher id
void Node::send_election()
{
    election_answered = false;
    // participants.lock();
    for (auto &[host, participant] : participants.map)
    {
        if (info.id < participant.id)
        {
            LOGF("Sending election message to %s id %d", host.c_str(), participant.id);
            int read = participant.socket->send("Ee" + padleft(std::to_string(info.id), 3));
            if (read < 0)
            {
                perrorcode("send");
                continue;
            }
        }
    }
    // participants.unlock();
}

bool Node::has_election_message(string &buffer)
{
    size_t idxE = buffer.find("E");
    if (idxE == string::npos)
        return false;
    return buffer[idxE + 1] == 'a' || buffer[idxE + 1] == 'c' || buffer[idxE + 1] == 'e';
}

void Node::handle_election_response(string &buffer)
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

// run in thread (maybe on listen, which might be on monitoring service)
// Answers election message of lower id node and starts election if it has not started
void Node::answer_election(int sender_id)
{
    if (info.id > sender_id)
    {
        // participants.lock();
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
            if (!election_running)
                run_election();
            // participants.unlock();
            return;
        }
        // participants.unlock();
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
    if (election_running)
        return;
    election_running = true;
    // Sends coordinator message if it has the highest id
    bool highest_id = true;
    // participants.lock();
    for (auto &[host, participant] : participants.map)
    {
        LOGF("My id %d, Peer %s id %d", info.id, participant.machine.to_string().c_str(), participant.id);
        if (!participant.status)
            continue;
        if (info.id < participant.id)
        {
            highest_id = false;
            break;
        }
    }
    // participants.unlock();
    if (highest_id)
    {
        LOGF("My id %d, is the largest", info.id);
        send_coordinator();
        change_manager(info.id);
        election_running = false;
        return;
    }
    LOGF("My id %d, is not the largest forwarding election message", info.id);
    // Else, send election message to all participants with higher id
    send_election();
    // Wait for answers
    msleep(TIMEOUT_ELECTION);
    if (election_answered)
    {
        msleep(TIMEOUT_COORDINATOR); // waits for coordinator message, if timeout, starts new election
        if (!received_coordinator)
        {
            election_running = false;
            goto restart_election;
            // run_election(); // might break the universe
        }
        else
        {
            election_running = false;
        }
        return;
    }
    // If no answer, send coordinator message
    LOGF("Everyone is dead so i must be the boss");
    send_coordinator();
    change_manager(info.id);
    election_running = false;
    LOG("Election finished");
}

#endif // NODE_IMPLEMENTATION