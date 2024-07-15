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
#include "str.h"
#include "Sockets/Sockets.h"
#include "commands.h"
#include "DataStructures/LockFreeQueue.h"

namespace DiscoveryService
{
    void start_dicovering(int port);
    void stop_dicovering(int port);
    Concurrent::LockFreeQueue<EndPoint> discovered_endpoints = {};
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
        char bff[MAXLINE];
        Str buffer = STR(bff);
        int read;
        while (1)
        {
            EndPoint ep = {};
            bzero(bff, MAXLINE);
            read = socket_receive_endpoint(&s, buffer, &ep, MSG_WAITALL);
            if (errno != 0)
            {
                perror("Error");
                pthread_cancel(thread);
                break;
            }

            if (str_cmp(str_take(buffer, read), client_msg) == 0)
            {
                EndPoint top = {};
                if(discovered_endpoints.peek(top) && epcmp_inaddr(&top, &ep) == 0) {
                    continue;
                }
                discovered_endpoints.enqueue(ep);
                socket_send_endpoint(&s, server_msg, &ep, MSG_DONTWAIT);
                s.error = 0;
            }
        }
        return NULL;
    }

    void *client_callback(void *data)
    {
        int port = (int)(intptr_t)data;
        char bff[MAXLINE];
        Str buffer = STR(bff);
        EndPoint braodcast_ep = ip_broadcast(port);
        while (1)
        {
            EndPoint ep = {};
            s.error = 0;
            socket_send_endpoint(&s, client_msg, &braodcast_ep, 0);
            msleep(1);
            int read = socket_receive_endpoint(&s, buffer, &ep, MSG_DONTWAIT);
            if (read < 0)
            {
                continue;
            }
            Str msg = str_take(buffer, read);
            if (str_cmp(msg, server_msg) == 0)
            {
                printf(str_fmt "\n", str_args(buffer));
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
        pthread_create(&thread, NULL, client_callback, (void *)(intptr_t)port);
    }

    void stop()
    {
        pthread_cancel(thread);
        s = Socket{};
    }
}
#endif // DISCOVERY_SERVICE_IMPLEMENTATION