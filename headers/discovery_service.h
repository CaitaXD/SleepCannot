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
    void start_server();
    void start_client();
    void stop();
    Concurrent::LockFreeQueue<MachineEndpoint> endpoints = {};
};

#ifdef DISCOVERY_SERVICE_IMPLEMENTATION

void DiscoveryService::start_server()
{
    running = true;
    pthread_create(&thread, NULL, [](void *data) -> void *
                   {
        DiscoveryService *ds = (DiscoveryService *)data;
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
                s.send(server_msg, client_machine, MSG_DONTWAIT);
            }
        }
        ds->running = false;
        return NULL; }, this);
}

void DiscoveryService::start_client()
{
    running = true;
    pthread_create(&thread, NULL, [](void *data) -> void *
                   {
        DiscoveryService *ds = std::move((DiscoveryService *)data);
        Socket s{};
        if (s.open(AdressFamily::InterNetwork, SocketType::Datagram, SocketProtocol::UDP) < 0)
        {
            perror("open");
            return NULL;
        }
        // if (s.bind(InternetAdress::Any, ds->port) < 0)
        // {
        //     perror("bind");
        //     return NULL;
        // }
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
            MacAddress mac = MacAddress::get_mac();
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
                MachineEndpoint top{INADDR_ANY, ds->port};
                if (!ds->endpoints.peek(top))
                {
                    ds->endpoints.enqueue(ep);
                }
                return NULL;
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
