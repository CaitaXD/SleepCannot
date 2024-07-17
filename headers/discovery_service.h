#ifndef _DISCOVERY_SERVICE_H
#define _DISCOVERY_SERVICE_H

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
#include "commands.h"
#include "macros.h"
#include "DataStructures/LockFreeQueue.h"

using string = std::string;
using string_view = std::string_view;
#define HOSTNAME_LEN 1024

class DiscoveryService
{
    pthread_t thread;
    bool running;
    int port;

public:
    DiscoveryService(int port) : port(port) {}
    ~DiscoveryService()
    {
        stop();
    }
    static void *server_callback(void *data);
    static void *client_callback(void *data);
    void start_server();
    void start_client();
    void stop();
    Concurrent::LockFreeQueue<MachineEndpoint> endpoints = {};
};
#endif // _DISCOVERY_SERVICE_H

#ifdef DISCOVERY_SERVICE_IMPLEMENTATION

void *DiscoveryService::server_callback(void *data)
{
    DiscoveryService *ds = (DiscoveryService *)data;
    defer(ds->running = false);
    Socket s{};
    s.open(AdressFamily::InterNetwork, SocketType::Datagram, SocketProtocol::UDP);
    if (s.bind(InternetAdress::Any, ds->port) < 0)
    {
        perror("bind");
        return NULL;
    }
    if (s.set_option(SO_BROADCAST, 1) < 0)
    {
        perror("set_option");
        return NULL;
    }
    string buffer;
    while (ds->running)
    {
        MachineEndpoint client_machine{};
        if (s.recv(&buffer, client_machine, MSG_DONTWAIT) < 0)
        {
            if (errno == EAGAIN)
            {
                continue;
            }
            perror("recv");
            break;
        }
        string_view buffer_view = string_view(buffer);
        std::cout << buffer << std::endl;
        if (buffer.rfind(client_msg))
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
            s.send(server_msg, client_machine, MSG_DONTWAIT);
        }
    }
    return NULL;
}

void *DiscoveryService::client_callback(void *data)
{
    DiscoveryService *ds = (DiscoveryService *)data;
    defer(ds->running = false);
    Socket s{};
    if (s.open(AdressFamily::InterNetwork, SocketType::Datagram, SocketProtocol::UDP) < 0)
    {
        perror("open");
        return NULL;
    }
    if (s.bind(InternetAdress::Any, ds->port) < 0)
    {
        perror("bind");
        return NULL;
    }
    if (s.set_option(SO_BROADCAST, 1) < 0)
    {
        perror("set_option");
        return NULL;
    }
    IpEndPoint braodcast_ep = IpEndPoint::broadcast(ds->port);
    string buffer;
    while (ds->running)
    {
        buffer.clear();
        MachineEndpoint ep{};
        MacAddress mac = get_mac();
        char hostname[HOSTNAME_LEN];
        gethostname(hostname, HOSTNAME_LEN);
        int hostname_len = strlen(hostname);

        buffer.append(client_msg);
        buffer.append((char *)&(hostname_len), sizeof(hostname_len));
        buffer.append(hostname, hostname_len);
        buffer.append((char *)mac.mac_addr, MAC_ADDR_MAX);
        buffer.append(mac.mac_str, MAC_STR_MAX);

        if (s.send(buffer, braodcast_ep) < 0)
        {
            perror("send");
            continue;
        }
        msleep(100);
        int read = s.recv(&buffer, ep, MSG_DONTWAIT);
        if (read < 0)
        {
            continue;
        }
        string_view msg = string_view(buffer.data(), read).substr(0, read);
        if (msg == server_msg)
        {
            std::cout << msg << std::endl;
            MachineEndpoint top{INADDR_ANY, ds->port};
            if (!ds->endpoints.peek(top))
            {
                ds->endpoints.enqueue(ep);
            }
            return NULL;
        }
    }
    return NULL;
}

void DiscoveryService::start_server()
{
    running = true;
    pthread_create(&thread, NULL, server_callback, this);
}

void DiscoveryService::start_client()
{
    running = true;
    pthread_create(&thread, NULL, client_callback, this);
}

void DiscoveryService::stop()
{
    running = false;
    pthread_join(thread, NULL);
}

#endif // DISCOVERY_SERVICE_IMPLEMENTATION
