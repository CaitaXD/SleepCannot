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


#define STR_IMPLEMENTATION
#include "str.h"
#undef STR_IMPLEMENTATION

#define SOCKET_IMPLEMENTATION
#include "Sockets/Sockets.h"
#undef SOCKET_IMPLEMENTATION

#define COMMANDS_IMPLEMENTATION
#include "commands.h"
#undef COMMANDS_IMPLEMENTATION

#define DISCOVERY_SERVICE_IMPLEMENTATION
#include "discovery_service.h"
#undef DISCOVERY_SERVICE_IMPLEMENTATION

bool key_hit()
{
  struct timeval tv = {0, 0};
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);
  return select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) == 1;
}

int server(int port)
{
  printf("Server Side\n\n");
  help_msg(NULL);
  DiscoveryService::start_server(port);
  std::vector<EndPoint> clients = {};
  Socket s = socket_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  while (1)
  {
    if (key_hit())
    {
      s.error |= parse_command(clients, &s);
    }
    if (s.error)
    {
      goto finally;
    }

    EndPoint ep = {};
    if (DiscoveryService::discovered_endpoints.dequeue(ep))
    {
      if(std::find_if(clients.begin(), clients.end(), [&ep](EndPoint other){ return epcmp_inaddr(&ep, &other); }) == clients.end())
      {
      	clients.push_back(ep);
      }
    }
  }
finally:
  DiscoveryService::stop();
  s.error |= close(s.fd);
  return s.error;
}

int client(int port)
{
  printf("Client Side\n\n");
  DiscoveryService::start_client(port);
  while (1)
  {
    msleep(1000);
  }
  DiscoveryService::stop();
  return 0;
}

int main(int argc, char **argv)
{
  if (argc <= 1)
  {
    printf("Usage: main <manager> if manager else <> [port (for testing)]\n");
    return -1;
  }
  bool is_server = !strcmp(argv[1], "manager");
  int port = is_server ? atoi(argv[2]) : atoi(argv[1]);

  ssize_t exit_code;
  if (is_server)
  {
    if ((exit_code = server(port)) != 0)
    {
      printf("Exit Code: %zi \n", exit_code);
    }
  }
  else
  {
    if ((exit_code = client(port)) != 0)
    {
      printf("Exit Code: %zi \n", exit_code);
    }
  }

  if (errno != 0)
  {
    perror("errno:");
  }
  return exit_code;
}
