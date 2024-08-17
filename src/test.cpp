#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <string>
#include <algorithm>
#include <thread>
#include <signal.h>
#include <unistd.h>

#define NODE_IMPLEMENTATION
#include "../headers/node.hpp"
#undef NODE_IMPLEMENTATION

#define NET_IMPLEMENTATION
#include "../headers/Net/Net.hpp"
#undef NET_IMPLEMENTATION

#define FILE_DESCRIPTOR_IMPLEMENTATION
#include "../headers/FileDescriptor.hpp"
#undef FILE_DESCRIPTOR_IMPLEMENTATION

#define SOCKET_IMPLEMENTATION
#include "../headers/Net/Socket.hpp"
#undef SOCKET_IMPLEMENTATION

#define DISCOVERY_SERVICE_IMPLEMENTATION
#include "../headers/discovery_service.h"
#undef DISCOVERY_SERVICE_IMPLEMENTATION

#define MONITORING_SERVICE_IMPLEMENTATION
#include "../headers/monitoring_service.h"
#undef MONITORING_SERVICE_IMPLEMENTATION

#define MANAGEMENT_IMPLEMENTATION
#include "../headers/management.hpp"
#undef MANAGEMENT_IMPLEMENTATION

#define COMMANDS_IMPLEMENTATION
#include "../headers/commands.hpp"
#undef COMMANDS_IMPLEMENTATION

char usage[] = "Usage: <client|server> <IP> <port>";

int client(std::string ip, int port);
int server(std::string ip, int port);

int main(int argc, char **argv)
{
    //signal(SIGPIPE, SIG_IGN);
    StringEqComparerIgnoreCase string_equals;

    if (argc != 4)
    {
        puts(usage);
        return -1;
    }

    std::string mode = argv[1];
    std::string ip = argv[2];
    int port = atoi(argv[3]);

    if (string_equals(mode, "client"))
    {
        return client(ip, port);
    }
    else if (string_equals(mode, "server"))
    {
        return server(ip, port);
    }
    else
    {
        puts(usage);
        return -1;
    }

    return 0;
}

int client(std::string ip, int port)
{
    int discovery_port = port + 0;
    int monitoring_port = port + 1;

    printf("Starting discovery client in port %d\n", discovery_port);
    DiscoveryService discovery_service;
    discovery_service.port = port;
    discovery_service.start_client();

    MachineEndpoint server_machine;
discovery:
    if (discovery_service.endpoints.dequeue(server_machine))
    {
        printf("Got server: %s\n", server_machine.to_string().c_str());
        goto monitoring;
    }
    goto discovery;
monitoring:
    Socket my_socket;
    int result = 0;
    result |= my_socket.open(AddressFamily::InterNetwork, SocketType::Stream, SocketProtocol::TCP);
    sleep(1);
    MachineEndpoint monitoring_server = server_machine.with_port(monitoring_port);
    result |= my_socket.connect(monitoring_server);

    if (result < 0)
    {
        perrorcode("connect");
        exit(EXIT_FAILURE);
    }

    printf("Connected to %s\n", monitoring_server.to_string().c_str());

    auto socket_ptr = std::make_shared<Socket>(std::move(my_socket));

    participant_t me{
        .machine = MachineEndpoint::MyMachine(InternetAddress::Any, port),
        .status = true,
        .socket = socket_ptr,
        .last_conection_timestamp = time(NULL),
        .id = 1
    };
    
    participant_t server{
        .machine = server_machine,
        .status = false,
        .socket = socket_ptr,
        .last_conection_timestamp = time(NULL),
        .id = 0
    };

    Node node{me, 0};
    node.participants.add(me);
    node.participants.add(server);
    node.info = me;
    MonitoringService monitoring_service{node};
    assert(!monitoring_service.node->is_manager());
    monitoring_service.start_service();

    while (1)
    {
        /* code */
    }
    

    return 0;
}

int server(std::string ip, int port)
{
    int discovery_port = port + 0;
    int monitoring_port = port + 1;

    printf("Starting discovery server in port %d\n", discovery_port);
    DiscoveryService discovery_service;
    discovery_service.port = port;
    discovery_service.start_server();

    MachineEndpoint client_machine;
discovery:
    if (discovery_service.endpoints.dequeue(client_machine))
    {
        printf("Got client: %s\n", client_machine.to_string().c_str());
        goto monitoring;
    }
    goto discovery;
monitoring:
    printf("Starting monitoring service on port %d\n", monitoring_port);
    Socket my_socket;

    int result = 0;
    result |= my_socket.open(AddressFamily::InterNetwork, SocketType::Stream, SocketProtocol::TCP);
    result |= my_socket.set_option(SO_REUSEADDR, 1);
    result |= my_socket.bind(monitoring_port);
    
    printf("%s", "Listening\n");

    result |= my_socket.listen();
    Socket client_socket = my_socket.accept(client_machine);
    result |= client_socket.file_descriptor;

    if (result < 0)
    {
        perrorcode("listen");
        exit(EXIT_FAILURE);
    }


    printf("Connection from %s\n", client_machine.to_string().c_str());

    client_socket.send("Hello there!");

    participant_t me{
        .machine = MachineEndpoint::MyMachine(InternetAddress::Any, port),
        .status = true,
        .socket = std::make_shared<Socket>(std::move(my_socket)),
        .last_conection_timestamp = time(NULL),
        .id = 0
    };

    participant_t client{
        .machine = client_machine,
        .status = false,
        .socket = std::make_shared<Socket>(std::move(client_socket)),
        .last_conection_timestamp = time(NULL),
        .id = 1
    };

    Node node{me};
    node.participants.add(me);
    node.participants.add(client);
    node.info = me;
    MonitoringService monitoring_service{node};
    assert(monitoring_service.node->is_manager());
    monitoring_service.start_service();

    while (1)
    {
        /* code */
    }

    return 0;
}
