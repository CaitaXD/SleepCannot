/*
  This service is used to discover other services on the local network
  It used UDP broadcasts to discover other services
  The client sends a packet containing a header its hostname and its mac address and waits for the server to respond with a header to then collect the endpoint of the server
*/
#ifndef DISCOVERY_SERVICE_H_
#define DISCOVERY_SERVICE_H_

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
#include <vector>
#include <pthread.h>
#include "commands.hpp"
#include "macros.h"
#include "DataStructures/LockFreeQueue.h"

using string = std::string;
using string_view = std::string_view;
#define HOSTNAME_LEN 1024

struct DiscoveryService
{
    pthread_t thread;
    bool running;
    int port;
    Socket udp_socket;
    Concurrent::LockFreeQueue<MachineEndpoint> endpoints = {};
    ~DiscoveryService()
    {
        stop();
    }
    void start_server();
    void start_client();
    void stop();
};

#endif // DISCOVERY_SERVICE_H_
#ifdef DISCOVERY_SERVICE_IMPLEMENTATION

void DiscoveryService::start_server()
{
    if (running)
    {
        return;
    }

    running = true;
    pthread_create(&thread, NULL, [](void *data) -> void *
                   {
        DiscoveryService *ds = (DiscoveryService *)data;
        Socket &server_socket = ds->udp_socket;
        server_socket.open(AddressFamily::InterNetwork, SocketType::Datagram, SocketProtocol::UDP);
        int result = server_socket.set_option(SO_BROADCAST, 1);
        result |= server_socket.set_option(SO_REUSEADDR, 1);
        result |= server_socket.bind(InternetAddress::Any, ds->port);
        if (result < 0)
        {
            perror("discovery start_server");
            return NULL;
        }
        string buffer;
        while (ds->running)
        {
            MachineEndpoint client_machine;
            if (server_socket.recv(&buffer, client_machine, MSG_DONTWAIT) < 0)
            {
                if (errno == EAGAIN)
                {
                    continue;
                }
                perror("recv");
                break;
            }
            string_view buffer_view = string_view(buffer);
            if (buffer_view.rfind(client_msg) == 0)
            {
                MachineEndpoint top{};
                if (ds->endpoints.peek(top) && top == client_machine)
                {
                    continue;
                }
                int cursor = client_msg.size();
                int client_hostname_len = *(int *)buffer_view.substr(cursor, sizeof(int)).data();
                cursor += sizeof(int);
    
                string_view client_hostname = buffer_view.substr(cursor, client_hostname_len);
                cursor += client_hostname_len;
    
                string_view client_mac_addr = buffer_view.substr(cursor, MAC_ADDR_MAX);
                cursor += MAC_ADDR_MAX;
    
                string_view client_mac_str = buffer_view.substr(cursor, MAC_STR_MAX);
                cursor += MAC_STR_MAX;
    
                memcpy(client_machine.mac.mac_addr, client_mac_addr.data(), MAC_ADDR_MAX);
                memcpy(client_machine.mac.mac_str, client_mac_str.data(), MAC_STR_MAX);
                client_machine.hostname = client_hostname;
    
                ds->endpoints.enqueue(client_machine);
                server_socket.send(server_msg, client_machine, MSG_DONTWAIT);
            }
        }
        ds->running = false;
        return NULL; }, this);
}

void DiscoveryService::start_client()
{
    if (running)
    {
        return;
    }

    running = true;
    pthread_create(&thread, NULL, [](void *data) -> void *
                   {
        string client_message;
        MacAddress mac = MacAddress::get_mac();
        string hostname = get_hostname();
        int hostname_len = hostname.length();

        client_message.append(client_msg);

        client_message.append((char *)&(hostname_len), sizeof(hostname_len));
        client_message.append(hostname);
        
        client_message.append((char *)mac.mac_addr, MAC_ADDR_MAX);
        client_message.append(mac.mac_str, MAC_STR_MAX);
        
        DiscoveryService *ds = std::move((DiscoveryService *)data);
        Socket &client_socket = ds->udp_socket;
        IpEndpoint braodcast_ep = IpEndpoint::broadcast(ds->port);
        
        int result = client_socket.open(AddressFamily::InterNetwork, SocketType::Datagram, SocketProtocol::UDP);
        result |= client_socket.bind(InternetAddress::Any, ds->port);
        result |= client_socket.set_option(SO_BROADCAST, 1);

        while (ds->running)
        {
            if (client_socket.send(client_message, braodcast_ep) < 0)
            {
                perror("send");
                continue;
            }
            msleep(100);
            MachineEndpoint server_endpoint;
            string recv_buffer;
            int read = client_socket.recv(&client_message, server_endpoint, MSG_DONTWAIT);
            if (read < 0)
            {
                continue;
            }
            string_view msg = string_view(client_message.data(), read).substr(0, read);
            if (msg == server_msg)
            {
                MachineEndpoint top{INADDR_ANY, ds->port};
                if (!ds->endpoints.peek(top))
                {
                    ds->endpoints.enqueue(server_endpoint);
                }
                return NULL;
            }
            else if (msg.rfind("wakeup") == 0) {
                std::cout << "Grab a brush and put a little makeup" << std::endl;
            }
        }
        ds->running = false;
        return NULL; }, this);
}

void DiscoveryService::stop()
{
    running = false;
    pthread_join(thread, NULL);
}

#endif // DISCOVERY_SERVICE_IMPLEMENTATION
