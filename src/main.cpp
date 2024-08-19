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
#include <thread>
#include <signal.h>
#include <unistd.h>

#define NET_IMPLEMENTATION
#include "../headers/Net/Net.hpp"
#undef NET_IMPLEMENTATION

#define FILE_DESCRIPTOR_IMPLEMENTATION
#include "../headers/FileDescriptor.hpp"
#undef FILE_DESCRIPTOR_IMPLEMENTATION

#define SOCKET_IMPLEMENTATION
#include "../headers/Net/Socket.hpp"
#undef SOCKET_IMPLEMENTATION

#define MONITORING_SERVICE_IMPLEMENTATION
#include "../headers/monitoring_service.h"
#undef MONITORING_SERVICE_IMPLEMENTATION

#define DISCOVERY_SERVICE_IMPLEMENTATION
#include "../headers/discovery_service.h"
#undef DISCOVERY_SERVICE_IMPLEMENTATION

#define MANAGEMENT_IMPLEMENTATION
#include "../headers/management.hpp"
#undef MANAGEMENT_IMPLEMENTATION

#define COMMANDS_IMPLEMENTATION
#include "../headers/commands.hpp"
#undef COMMANDS_IMPLEMENTATION

#define NODE_IMPLEMENTATION
#include "../headers/node.hpp"
#undef NODE_IMPLEMENTATION

StringEqComparerIgnoreCase string_equals;

bool is_server = false;

// SIGINT handler for properly exiting the program
void cleanup(int signum)
{
  (void)signum;
  int errno_save = errno;
  if (is_server)
  {
    exit(EXIT_FAILURE);
  }
  else
  {
    signal(signum, SIG_DFL);
    raise(SIGINT);
  }
  errno = errno_save;
}


void close_fd(int sig, siginfo_t *sig_info, void *data) {
  (void)sig_info;
  (void)sig;
  (void)data;
}

int main(int argc, char **argv)
{
  signal(SIGPIPE, SIG_IGN); 

  if (argc < 1 || argc > 2)
  {
    printf("Usage: main <manager> if manager else <> for participant\n");
    return -1;
  }

#ifdef LOG_ENABLE
  std::cout << "Logging enabled" << std::endl;
#endif

  is_server = argc > 1 && string_equals(argv[1], "manager");

  struct sigaction sigint;
  sigint.sa_handler = cleanup;
  sigint.sa_flags = SA_RESTART;
  sigaction(SIGINT, &sigint, NULL);

  struct sigaction sigpipe;
  sigpipe.sa_sigaction = close_fd;
  sigaction(SIGPIPE, &sigpipe, NULL);

  Node node(is_server);
  node.run_node();

  return 0;
}
