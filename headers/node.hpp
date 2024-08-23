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
class MonitoringService *monitoring_service_create(class Node *node);
void monitoring_service_start(class MonitoringService *ms);
void monitoring_service_stop(class MonitoringService *ms);
void monitoring_service_mark_as_deleted(class MonitoringService *ms, const string &host);

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
#include "Net/Server.hpp"
#include "Net/Client.hpp"

#define INITIAL_ID 1000
#define CLEAR_SCREEN "\033[2J" // ascii escape code to clear the screen

enum ElectionState
{
    NoElection,
    Running,
    Overruled,
};

struct Message
{
    string payload;
    Peer sender;
    time_t timestamp;
};

extern std::vector<string> hosts_marked_for_removal;

class Node
{
public:
    string hostname;
    ParticipantTable participants = {};
    int manager_id = -1;
    ElectionState election_state = NoElection;
    int highest_id_in_election = -1;
    time_t election_start_time = 0;
    DiscoveryService discover_peers_service = {};
    MonitoringService *monitoring_service = {};
    SocketServer server = {};

    Node(bool is_server);
    ~Node();

    Concurrent::LockFreeQueue<Message> message_queue = {};

    Peer &get_info();
    void start();
    void stop();
    bool is_manager();
    void change_manager(int new_manager_id);
    bool my_info(Peer &participant);
    bool my_file_descriptor(int fd);
    int last_peer_id();
    void enqueue_messages(int poll_events = POLLIN, int timeout = 5000);
    void listen(int backlog = 20);
    void accept_peers();
    void connect_peers();
    void close_peers();
    void remove_dead_peers();
    void send_to_peers(const string &msg, std::function<bool(Peer &)> predicate);
    std::vector<pollfd> poll(int poll_events, int timeout);

private:
    pthread_t _server_thread = {};
};

#endif // NODE_H_

#ifndef NODE_IMPLEMENTATION
#define NODE_IMPLEMENTATION

Peer &Node::get_info()
{
    return participants[hostname];
}

Node::Node(bool is_server)
{
    discover_peers_service.port = DISCOVERY_PORT;
    monitoring_service = monitoring_service_create(this);
    MachineEndpoint local_machine = MachineEndpoint::MyMachine(InternetAddress::Loopback, TCP_SERVER_PORT);
    hostname = local_machine.hostname;

    Peer info;
    info.is_manager = is_server;
    info.status = true;
    info.last_conection_timestamp = time(NULL);
    info.machine = local_machine;
    info.client_socket = std::make_shared<Socket>();
    info.id = is_server ? INITIAL_ID : -1;

    LOGF("My machine: %s", info.machine.to_string().c_str());
    if (is_server)
    {
        info.id = INITIAL_ID;
        manager_id = info.id;
    }
    else
    {
        manager_id = INITIAL_ID;
    }

    participants.add(info);
}

Node::~Node()
{
    pthread_join(this->_server_thread, NULL);
    free(this->monitoring_service);
}

void Node::change_manager(int new_manager_id)
{
    auto &info = get_info();
    if (is_manager())
    {
        discover_peers_service.stop();
        info.is_manager = false;
        discover_peers_service.start_client();
    }

    if (new_manager_id == info.id)
    {
        discover_peers_service.stop();
        info.is_manager = true;
        discover_peers_service.start_server();
    }

    auto &old_manager = participants.find_id_blocking(manager_id);
    old_manager.is_manager = false;

    auto &new_manager = participants.find_id_blocking(new_manager_id);
    new_manager.is_manager = true;
    new_manager.last_conection_timestamp = time(NULL);

    participants.dirty = true;
    manager_id = new_manager_id;
}

void Node::start()
{
    StringEqComparerIgnoreCase string_equals;
    monitoring_service_start(monitoring_service);
    listen(20);
restart:
    if (is_manager())
    {
        LOGF("Starting as manager");
        discover_peers_service.start_server();
        help_msg_server();
        {
            participants.read_lock();
            participants.print();
        }
        while (is_manager())
        {
            // if (key_hit())
            // {
            //     command_exec(participants);
            // }
            if (participants.dirty)
            {
                // std::cout << CLEAR_SCREEN << "Manager\n";
                help_msg_server();
                {
                    participants.read_lock();
                    participants.print();
                }
            }
        }
    }
    else
    {
        discover_peers_service.start_client();
        NetworkInterfaceList network_interfaces = NetworkInterfaceList::begin();
        std::printf("MAC ADDRESS: %s\nHOSTNAME: %s\n%s\n", MacAddress::get_mac().mac_str, get_hostname().c_str(), network_interfaces->to_string().c_str());
        help_msg_client();
        while (!is_manager())
        {
            // Read lock
            {
                participants.read_lock();
                auto &info = get_info();
                if (key_hit())
                {
                    string cmd;
                    std::cin >> cmd;
                    if (string_equals(cmd, "EXIT"))
                    {
                        info.client_socket->send("exit");
                        exit(EXIT_SUCCESS);
                    }
                }
            }
        }
    }
    discover_peers_service.stop();
    goto restart;
}

void Node::send_to_peers(const string &msg, std::function<bool(Peer &)> predicate)
{
    for (auto &[host, peer] : participants)
    {
        assert(peer.client_socket != nullptr);
        // assert(peer.client_socket->is_open());
        if (predicate(peer) && peer.client_socket->is_open() && !my_info(peer))
        {
            LOGF("Sending %s to %d %s", msg.c_str(), peer.id, peer.machine.to_string().c_str());
            int r = peer.client_socket->send(msg);
            if (r < 0)
            {
                std::error_printf(errno, "send to %s [FD %d]", peer.machine.hostname.c_str(), peer.client_socket->file_descriptor);
            }
        }
    }
}

void Node::close_peers()
{
    assert(false);
    for (auto &[host, peer] : participants)
    {
        peer.client_socket->close();
    }
}

void Node::connect_peers()
{
    MachineEndpoint discoveredMachine;
    while (discover_peers_service.endpoints.dequeue(discoveredMachine))
    {
        const auto &info = get_info();
        auto it = participants.find(discoveredMachine.hostname);
        if (it == participants.end())
        {
            Peer peer;
            bool im_manager = is_manager();
            peer.machine = discoveredMachine;
            peer.status = true;
            peer.last_conection_timestamp = time(NULL);
            peer.id = im_manager ? (last_peer_id() - 1) : INITIAL_ID;
            peer.is_manager = !im_manager;
            auto machine = discoveredMachine.with_port(TCP_SERVER_PORT);
            peer.client_socket = std::make_shared<Socket>();
            if (peer.id > info.id)
            {
                auto client = TcpClient::connect(AddressFamily::IPv4, SocketType::Stream, SocketProtocol::TCP, machine);
                if (client.is_error())
                {
                    std::error_printf(client.error(), "SocketClient::create");
                    continue;
                }
                *peer.client_socket = std::move(client.value().socket);
                LOGF("Discovered and Connected to %s [FD %d]", peer.machine.to_string().c_str(), peer.client_socket->file_descriptor);
            }
            LOGF("Adding %s", peer.machine.to_string().c_str());
            participants.add(peer);
        }
    }

    for (auto &[host, peer] : participants)
    {
        assert(peer.client_socket != nullptr);
        if (peer.client_socket->is_closed() && peer.id > get_info().id)
        {
            auto machine = peer.machine.with_port(TCP_SERVER_PORT);
            auto client = TcpClient::connect(AddressFamily::IPv4, SocketType::Stream, SocketProtocol::TCP, machine);
            if (client.is_error())
            {
                std::error_printf(client.error(), "SocketClient::connect");
                continue;
            }
            *peer.client_socket = std::move(client.value().socket);
            LOGF("Connected to %s [FD %d]", peer.machine.to_string().c_str(), peer.client_socket->file_descriptor);
        }
    }
}

void Node::enqueue_messages(int poll_events, int timeout)
{
    auto pollin = poll(poll_events, timeout);
    for (auto &fd : pollin)
    {
        if (fd.fd < 0)
            continue;
        Socket socket(fd.fd);
        socket.keep_open_on_destructor = true;
        Peer peer = participants.find_socket_blocking(socket);
        assert(peer.client_socket != nullptr);
        string payload;
        int bytes_received = socket.recv(&payload);
        if (bytes_received > 0)
        {
            message_queue.enqueue({
                .payload = payload,
                .sender = peer,
                .timestamp = time(NULL),
            });
        }
    }
}

std::vector<pollfd> Node::poll(int poll_events = POLLIN, int timeout = 0)
{
    std::vector<pollfd> fds;
    for (auto &[host, peer] : participants)
    {
        if (peer.client_socket->is_open() && !my_info(peer))
        {
            pollfd fd;
            fd.fd = peer.client_socket->file_descriptor;
            fd.events = poll_events;
            fds.push_back(fd);
        }
    }

    int num_events = ::poll(fds.data(), fds.size(), timeout);
    if (num_events == 0)
    {
        return std::vector<pollfd>{};
    }

    std::vector<pollfd> poll_result = std::vector<pollfd>();
    for (nfds_t i = 0; i < fds.size(); i++)
    {
        if (fds[i].revents & poll_events)
        {
            poll_result.push_back(fds[i]);
        }
    }

    return poll_result;
}

int Node::last_peer_id()
{
    int last_id = INITIAL_ID;
    for (auto &[host, participant] : participants)
    {
        if (participant.id < last_id)
            last_id = participant.id;
    }
    return last_id;
}

bool Node::is_manager()
{
    return manager_id == get_info().id;
}

bool Node::my_info(Peer &participant)
{
    return participant.id == get_info().id;
}

void Node::listen(int backlog)
{
    IpEndpoint server_ep{InternetAddress::Any, TCP_SERVER_PORT};
    auto server_result = SocketServer::serve(AddressFamily::IPv4, SocketType::Stream, SocketProtocol::TCP, server_ep, backlog);
    if (server_result.is_error())
    {
        std::error_printf(server_result.error(), "SocketServer::start");
        exit(EXIT_FAILURE);
    }
    server = std::move(server_result.value());
}

void Node::accept_peers()
{
    auto &info = get_info();
    if (info.client_socket->is_closed())
    {
        info.client_socket = std::make_shared<Socket>(std::move(server.socket));
    }
    auto &node_info = get_info();
    auto &socket = node_info.client_socket;
    // LOGF("Listening on %s", server.endpoint.to_string().c_str());
    IpEndpoint peerAddress;

    while ((socket->poll(POLLIN, 1).revents & POLLIN))
    {
        LOGF("Pending connection");
        auto client = socket->accept(peerAddress);
        if (client.file_descriptor < 0 && client.lasterrno != 0)
        {
            std::error_printf(client.lasterrno, "SocketServer::accept");
            return;
        }
        auto &peer = participants.find_address_blocking(peerAddress);

        if (peer.id > node_info.id)
        {
            LOGF("Rejecting connection from %s %d", peer.machine.to_string().c_str(), peer.id);
            client.close();
            return;
        }
        *peer.client_socket = std::move(client);
        LOGF("Accepted Connection from  %s %d", peer.machine.to_string().c_str(), peer.id);
    }
};

void Node::remove_dead_peers()
{
    for (auto &host : hosts_marked_for_removal)
    {
        participants.remove(host);
    }
    hosts_marked_for_removal.clear();
}

bool Node::my_file_descriptor(int fd)
{
    return get_info().client_socket->file_descriptor == fd;
}

#endif // NODE_IMPLEMENTATION