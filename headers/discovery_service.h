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
#include <pthread.h>
#include <vector>
#include "DataStructures/str.h"
#include "Sockets.h"
#include "commands.h"
#include "DataStructures/LockFreeQueue.h"

namespace DiscoveryService
{
    void start_server(int port);
    void start_client(int port);
    void stop();
    Concurrent::LockFreeQueue<MachineEndpoint> discovered_endpoints = {};
}
#endif // _DISCOVERY_SERVICE_H

#ifdef DISCOVERY_SERVICE_IMPLEMENTATION

static inline int msleep(long msec)
{
    struct timespec ts;
    int res;
    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }
    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;
    do
    {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);
    return res;
}

namespace DiscoveryService
{
    Socket s;
    pthread_t thread;

    void *sever_callback(void *data)
    {
        (void)data;
        char buffer[MAXLINE];
        Str buffer_view = STR(buffer);
        int read;
        while (1)
        {
            MachineEndpoint ep = {};
            bzero(buffer, MAXLINE);
            read = socket_receive_endpoint(&s, buffer_view, &ep, MSG_WAITALL);
            if (errno != 0)
            {
                perror("Error");
                pthread_cancel(thread);
                break;
            }

            if (str_starts_with(str_take(buffer_view, read), client_msg) == 0)
            {
                MachineEndpoint top = {};
                if (discovered_endpoints.peek(top) && epcmp_inaddr(&top, &ep) == 0)
                {
                    continue;
                }
                int cursor = client_msg.len;
                int client_hostname_len = *(int *)(str_slice(buffer_view, cursor, sizeof(int)).data);
                cursor += sizeof(int);

                Str client_hostname = str_slice(buffer_view, cursor, client_hostname_len);
                cursor += client_hostname_len;

                Str client_mac_addr = str_slice(buffer_view, cursor, MAC_ADDR_MAX);
                cursor += MAC_ADDR_MAX;

                Str client_mac_str = str_slice(buffer_view, cursor, MAC_STR_MAX);
                cursor += MAC_STR_MAX;

                memcpy(ep.mac.mac_addr, client_mac_addr.data, MAC_ADDR_MAX);
                memcpy(ep.mac.mac_str, client_mac_str.data, MAC_STR_MAX);
                ep.hostname = string_from_str(client_hostname);
                
                discovered_endpoints.enqueue(ep);
                socket_send_endpoint(&s, server_msg, &ep, MSG_DONTWAIT);
                s.error = 0;
            }
        }
        return NULL;
    }

#define HOSTNAME_LEN 1024
    void *client_callback(void *data)
    {
        int port = (int)(intptr_t)data;
        std::string buffer = std::string();
        EndPoint braodcast_ep = ip_broadcast(port);
        while (1)
        {
            buffer.clear();
            MachineEndpoint ep = {};
            s.error = 0;
            MacAddress mac = get_mac();
            char hostname[HOSTNAME_LEN];
            gethostname(hostname, HOSTNAME_LEN);
            int hostname_len = strlen(hostname);

            buffer.append(str_unpack(client_msg));
            buffer.append((char *)&(hostname_len), sizeof(hostname_len));
            buffer.append(hostname, hostname_len);
            buffer.append((char *)mac.mac_addr, MAC_ADDR_MAX);
            buffer.append(mac.mac_str, MAC_STR_MAX);

            Str buffer_view = str_from_string(buffer);
            socket_send_endpoint(&s, buffer_view, &braodcast_ep, 0);
            msleep(1);
            int read = socket_receive_endpoint(&s, buffer_view, &ep, MSG_DONTWAIT);
            if (read < 0)
            {
                continue;
            }
            Str msg = str_take(buffer_view, read);
            if (str_cmp(msg, server_msg) == 0)
            {
                printf(str_fmt "\n", str_args(msg));
                MachineEndpoint top = {};
                if (!discovered_endpoints.peek(top)) {
                    discovered_endpoints.enqueue(ep);
                }
                sleep(60);
            }
        }
        return NULL;
    }

    void start_server(int port)
    {
        s = socket_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        int broadcastEnable = 1;
        int ret = setsockopt(s.fd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
        //printf("%d", ret);
        if (ret < 0)
        {
            perror("start_server");
            return;
        }
        socket_bind(&s, ip_endpoint(INADDR_ANY, port));
        pthread_create(&thread, NULL, sever_callback, NULL);
    }

    void start_client(int port)
    {
        s = socket_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        int broadcastEnable = 1;
        int ret = setsockopt(s.fd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
        if (ret < 0)
            perror("start_client");
        //socket_bind(&s, ip_endpoint(INADDR_ANY, port));
        pthread_create(&thread, NULL, client_callback, (void *)(intptr_t)port);
    }

    void stop()
    {
        pthread_cancel(thread);
        close(s.fd);
        s = Socket{};
    }
}
#endif // DISCOVERY_SERVICE_IMPLEMENTATION
