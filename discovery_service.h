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
    void start_dicovering();
    void stop_dicovering();
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
            read = socket_receive_endpoint(&s, buffer, &ep, 0);
            if (s.error)
            {
                perror("Error");
                pthread_cancel(thread);
                break;
            }

            if (str_cmp(str_take(buffer, read), client_msg) == 0)
            {
                discovered_endpoints.enqueue(ep);
                socket_send_endpoint(&s, server_msg, &ep, MSG_DONTWAIT);
                s.error = 0;
            }
        }
        return NULL;
    }

    void *client_callback(void *data)
    {
        (void)data;
        char bff[MAXLINE];
        Str buffer = STR(bff);
        EndPoint braodcast_ep = ip_broadcast(PORT);
        bool admited = false;
        while (1)
        {
            EndPoint ep = {};
            if (!admited)
            {
                socket_send_endpoint(&s, client_msg, &braodcast_ep, 0);
            }
            int read = socket_receive_endpoint(&s, buffer, &ep, 0);
            Str msg = str_take(buffer, read);
            if (str_cmp(msg, server_msg) == 0)
            {
                break;
            }
        }
        return NULL;
    }

    void start_server()
    {
        s = socket_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        int broadcastEnable = 1;
        int ret = setsockopt(s.fd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
        if (ret < 0)
        {
            perror("start_server");
            return;
        }
        socket_bind(&s, ip_endpoint(INADDR_ANY, PORT));
        pthread_create(&thread, NULL, sever_callback, NULL);
    }

    void start_client()
    {
        s = socket_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        int broadcastEnable = 1;
        int ret = setsockopt(s.fd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
        if (ret < 0)
            perror("start_client");
        socket_bind(&s, ip_endpoint(INADDR_ANY, PORT));
        pthread_create(&thread, NULL, client_callback, NULL);
    }

    void stop()
    {
        pthread_cancel(thread);
        s = Socket{};
    }
}
#endif // DISCOVERY_SERVICE_IMPLEMENTATION