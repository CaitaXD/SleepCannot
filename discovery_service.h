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

void ds_start_dicovering(Socket s);
void ds_stop_dicovering();


#endif // _DISCOVERY_SERVICE_H

#ifdef DISCOVERY_SERVICE_IMPLEMENTATION

Clients ds_discovered_clients = std::vector<EndPoint>();
Socket in_ds_socket;

EndPoint ds_endpoint;

pthread_t ds_discovery_thread;
pthread_t ds_sendep_thread;

static inline bool contains(std::vector<EndPoint> v, EndPoint ep, int (*cmp)(EndPoint *lhs, EndPoint *rhs)) {
    for (size_t i = 0; i < v.size(); i++) {
        if (cmp(&v[i], &ep) == 0) {
            return true;
        }
    }
    return false;
}

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
    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);
    return res;
}

void* ds_broadcast_thread_callback(void* data) {
    (void)data;
    while (1) {
        socket_send_endpoint(&in_ds_socket, client_msg, &ds_endpoint, 0);
        msleep(500);
    }
    return NULL;
}

void* ds_server_thread_callback(void* data) {
    (void)data;
    char bff[MAXLINE];
    Str buffer = STR(bff);
    int read;
    while (1) {
        read = socket_receive_endpoint(&in_ds_socket, buffer, &ds_endpoint, MSG_DONTWAIT);
        if(errno == EAGAIN) {
          in_ds_socket.error = 0;
          continue;
        }
        if (in_ds_socket.error) {
          pthread_cancel(ds_discovery_thread);
          pthread_cancel(ds_sendep_thread);
          perror("Discovery Service Error Server messager");
          break;
        }

        if (str_cmp(str_take(buffer, read), client_msg) == 0) {
            if (!contains(ds_discovered_clients, ds_endpoint, epcmp_inaddr)) {
              struct sockaddr_in *addr = (struct sockaddr_in *)&ds_endpoint.addr;
              const char *ip = inet_ntoa(addr->sin_addr);
              int port = ntohs(addr->sin_port);
              printf("%s %s:%d\n", "New client, IP:", ip, port);
              ds_discovered_clients.push_back(ds_endpoint);
            }
        }
    }
    return NULL;
}

void ds_start_dicovering(Socket s) {
    pthread_create(&ds_sendep_thread, NULL, ds_broadcast_thread_callback, NULL);
    pthread_create(&ds_discovery_thread, NULL, ds_server_thread_callback, NULL);
    in_ds_socket = s;
    ds_endpoint = ip_broadcast(PORT);
}

void ds_stop_dicovering() {
    pthread_cancel(ds_discovery_thread);
    pthread_cancel(ds_sendep_thread);
    in_ds_socket = (Socket){};
    ds_endpoint = (EndPoint){};
}

#endif //DISCOVERY_SERVICE_IMPLEMENTATION