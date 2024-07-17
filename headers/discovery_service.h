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
#include <thread>
#include <vector>
#include "DataStructures/str.h"
#include "commands.h"
#include "DataStructures/LockFreeQueue.h"

#define HOSTNAME_LEN 1024

class DiscoveryService
{
    std::thread thread;
    bool running;

public:
    void start_server(int port);
    void start_client(int port);
    void stop();
    Concurrent::LockFreeQueue<MachineEndpoint> endpoints = {};
};
#endif // _DISCOVERY_SERVICE_H

#ifdef DISCOVERY_SERVICE_IMPLEMENTATION

void DiscoveryService::start_server(int port)
{
    thread = std::thread([&]() {
        Socket s{};
        s.open(AdressFamily::InterNetwork, SocketType::Datagram, SocketProtocol::UDP);
        if (s.bind(InternetAdress::Any, port) < 0)
        {
            perror("bind");
            return;
        }
        running = true;
        std::string buffer;
        while (running) {
            MachineEndpoint client_machine{};
            if (s.recv(&buffer, client_machine, MSG_DONTWAIT) < 0) {
                if (errno == EAGAIN)
                {
                    continue;
                }
                perror("recv");
                break;
            }
            std::string_view buffer_view = std::string_view(buffer);
            std::cout << buffer << std::endl;
            if (buffer.rfind(client_msg)) {
                MachineEndpoint top{};
                if (endpoints.peek(top) && top == client_machine)
                {
                    continue;
                }
                int cursor = client_msg.size();
                int client_hostname_len = *(int *)buffer_view.substr(cursor, sizeof(int)).data();
                cursor += sizeof(int);

                std::string_view client_hostname = buffer_view.substr(cursor, client_hostname_len);
                cursor += client_hostname_len;

                std::string_view client_mac_addr = buffer_view.substr(cursor, MAC_ADDR_MAX);
                cursor += MAC_ADDR_MAX;

                std::string_view client_mac_str = buffer_view.substr(cursor, MAC_STR_MAX);
                cursor += MAC_STR_MAX;

                memcpy(client_machine.mac.mac_addr, client_mac_addr.data(), MAC_ADDR_MAX);
                memcpy(client_machine.mac.mac_str, client_mac_str.data(), MAC_STR_MAX);
                client_machine.hostname = client_hostname;

                endpoints.enqueue(client_machine);
                s.send(server_msg, client_machine, MSG_DONTWAIT);
            }
        }
        running = false;
        s.close(); 
    });
}

void DiscoveryService::start_client(int port)
{
    thread = std::thread([this, port]() {
        Socket s{};
        if(s.open(AdressFamily::InterNetwork, SocketType::Datagram, SocketProtocol::UDP) < 0)
        {
            perror("open");
            return;
        }
        if (s.bind(InternetAdress::Any, port) < 0){
            perror("bind");
            return;
        }
        if(s.set_option(SO_BROADCAST, 1) < 0)
        {
            perror("set_option");
            return;
        }
        running = true;
        IpEndPoint braodcast_ep = IpEndPoint::broadcast(port);
        std::string buffer = std::string();
        while (running)
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

            if(s.send(buffer, braodcast_ep) < 0) {
                perror("send");
                continue;
            }
            msleep(100);
            int read = s.recv(&buffer, ep, MSG_DONTWAIT);
            if (read < 0)
            {
                continue;
            }
            std::string_view msg = std::string_view(buffer.data(), read).substr(0, read);
            if (msg == server_msg)
            {
                std::cout << msg << std::endl;
                MachineEndpoint top = {INADDR_ANY, port};
                if (!endpoints.peek(top))
                {
                    endpoints.enqueue(ep);
                }
                sleep(60);
            }
        }
        running = false; });
}

void DiscoveryService::stop()
{
    running = false;
    thread.join();
}

#endif // DISCOVERY_SERVICE_IMPLEMENTATION
