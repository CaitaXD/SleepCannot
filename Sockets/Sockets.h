#ifndef SOCKET_H_
#define SOCKET_H_

#include "../DataStructures/str.h"
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct EndPoint
{
  struct sockaddr addr;
  socklen_t addrlen;
} EndPoint;

typedef struct Socket
{
  ssize_t error;
  int fd;
  sa_family_t domain;
  int type;
  int protocol;
} Socket;

// Create a internet protocol endpoint with a specific address and port
EndPoint ip_endpoint(in_addr_t address, int port);
// Compare two endpoints by their address
int epcmp_inaddr(EndPoint *a, EndPoint *b);
// Create a new socket with a specific domain, type and protocol
Socket socket_create(sa_family_t domain, int type, int protocol);
// Bind a socket to a specific endpoint
ssize_t socket_bind(Socket *s, EndPoint endpoint);
// Send a message to a specific endpoint.
// Used for non conection oriented sockets
ssize_t socket_send_endpoint(Socket *s, Str message, EndPoint *ep, int flags);
// Receive a message from a specific endpoint.
// Used for non conection oriented sockets
ssize_t socket_receive_endpoint(Socket *s, Str buffer, EndPoint *ep, int flags);

#define EORROR_SOCKET_NEW -1
#define ERROR_SOCKET_BIND -2
#define ERROR_SOCKET_SEND -3
#define ERROR_SOCKET_RECEIVE -4

#endif // SOCKET_H_
#ifdef SOCKET_IMPLEMENTATION

in_addr_t to_in_adrr(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
{
  return (a << 24) | (b << 16) | (c << 8) | d;
}

EndPoint ip_endpoint(in_addr_t address, int port)
{
  struct sockaddr_in sockaddr = {};
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_addr.s_addr = htonl(address);
  sockaddr.sin_port = htons(port);

  return (EndPoint){
      .addr = *(struct sockaddr *)&sockaddr,
      .addrlen = sizeof(struct sockaddr_in),
  };
}

EndPoint ip_broadcast(int port)
{
  struct sockaddr_in sockaddr = {};
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_addr.s_addr = INADDR_BROADCAST;
  sockaddr.sin_port = htons(port);

  return (EndPoint){
      .addr = *(struct sockaddr *)&sockaddr,
      .addrlen = sizeof(struct sockaddr_in),
  };
}


ssize_t socket_bind(Socket *s, EndPoint endpoint)
{
  if (s->error != 0)
  {
    return s->error;
  }

  int r = bind(s->fd, &endpoint.addr, endpoint.addrlen);
  if (r < 0)
  {
    s->error = ERROR_SOCKET_BIND;
  }

  return r;
}

Socket socket_create(sa_family_t domain, int type, int protocol)
{
  Socket s = {
      .error = 0,
      .fd = 0,
      .domain = domain,
      .type = type,
      .protocol = protocol,
  };

  int sockfd = socket(domain, type, protocol);
  s.fd = sockfd;
  if (sockfd < 0)
  {
    s.error = EORROR_SOCKET_NEW;
    return s;
  }
  return s;
}

ssize_t socket_send_endpoint(Socket *s, Str message, EndPoint *ep, int flags)
{
  if (s->error)
  {
    return s->error;
  }
  ep->addrlen = sizeof(sockaddr_in);
  ssize_t n = sendto(s->fd, message.data, message.len, flags, &ep->addr, ep->addrlen);

  if (n < 0)
  {
    s->error = ERROR_SOCKET_SEND;
  }

  return n;
}

ssize_t socket_receive_endpoint(Socket *s, Str buffer, EndPoint *ep, int flags)
{
  if (s->error)
  {
    return s->error;
  }
  ep->addrlen = sizeof(sockaddr_in);
  ssize_t n = recvfrom(s->fd, (char *)str_unpack(buffer), flags, &ep->addr, &ep->addrlen);
  if (n < 0 && errno != EAGAIN)
  {
    s->error = ERROR_SOCKET_RECEIVE;
  }
  return n;
}

int epcmp_inaddr(EndPoint *a, EndPoint *b)
{
  struct sockaddr_in *a_in = (struct sockaddr_in *)&a->addr;
  struct sockaddr_in *b_in = (struct sockaddr_in *)&b->addr;

  return a_in->sin_addr.s_addr - b_in->sin_addr.s_addr;
}

#endif // SOCKET_IMPLEMENTATION