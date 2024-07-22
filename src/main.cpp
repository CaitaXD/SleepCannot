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

#define DISCOVERY_SERVICE_IMPLEMENTATION
#include "../headers/discovery_service.h"
#undef DISCOVERY_SERVICE_IMPLEMENTATION

#define MONITORING_SERVICE_IMPLEMENTATION
#include "../headers/monitoring_service.h"
#undef MONITORING_SERVICE_IMPLEMENTATION

#define MANAGEMENT_IMPLEMENTATION
#include "../headers/management.hpp"
#undef MANAGEMENT_IMPLEMENTATION

#define COMMANDS_IMPLEMENTATION
#include "../headers/commands.hpp"
#undef COMMANDS_IMPLEMENTATION

bool key_hit()
{
  struct timeval tv = {0, 0};
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);
  return select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) == 1;
}

DiscoveryService discovery_service;
MonitoringService monitoring_service;
bool is_server = false;

void cleanup(int signum)
{
  (void)signum;
  int errno_save = errno;
  if (is_server)
  {
    monitoring_service.tcp_socket.close();
    exit(EXIT_FAILURE);
  }
  else
  {
    monitoring_service.tcp_socket.send("exit");
    signal(signum, SIG_DFL);
    raise(SIGINT);
  }
  errno = errno_save;
}

#define CLEAR_SCREEN "\033[2J"
int server()
{
  ParticipantTable participants;
  discovery_service.start_server();
  monitoring_service.start_server(participants);

  help_msg_server();
  participants.print();

  while (1)
  {
    if (key_hit())
    {
      command_exec(participants);
    }

    participants.lock();

    if (participants.dirty)
    {
      std::cout << CLEAR_SCREEN << "Manager\n";
      help_msg_server();
      participants.print();
    }

    MachineEndpoint discoveredMachine;
    if (discovery_service.endpoints.dequeue(discoveredMachine))
    {
      participants.add(participant_t{
          .machine = discoveredMachine,
          .status = true,
          .socket = std::make_shared<Socket>(),
          .last_conection_timestamp = time(NULL)});
    }

    participants.unlock();
  }
  return 0;
}

int client()
{
  NetworkInterfaceList network_interfaces = NetworkInterfaceList::begin();
  std::cout << "MAC ADDRESS: " << MacAddress::get_mac().mac_str << "\nHOSTNAME: " << get_hostname() << "\n"
            << network_interfaces->to_string() << std::endl;
  help_msg_client();
  discovery_service.start_client();

  while (1)
  {
    if (key_hit())
    {
      string cmd;
      std::cin >> cmd;
      if (cmd == "exit")
      {
        monitoring_service.tcp_socket.send("exit");
        exit(EXIT_SUCCESS);
      }
    }
    MachineEndpoint server_machine_endpoint;
    if (!monitoring_service.running && discovery_service.endpoints.dequeue(server_machine_endpoint))
    {
      monitoring_service.start_client(server_machine_endpoint);
    }
  }
  return 0;
}

int main(int argc, char **argv)
{
  if (argc < 1 || argc > 2)
  {
    printf("Usage: main <manager> if manager else <> for participant\n");
    return -1;
  }
  is_server = argc > 1 && !strcmp(argv[1], "manager");

  struct sigaction sa;
  sa.sa_handler = cleanup;
  sa.sa_flags = SA_RESTART;
  sigaction(SIGINT, &sa, NULL);

  discovery_service.port = INITIAL_PORT + 50;
  monitoring_service.port = INITIAL_PORT + 51;

  ssize_t exit_code;
  if (is_server)
  {
    if ((exit_code = server()) != 0)
    {
      printf("Exit Code: %zi \n", exit_code);
    }
  }
  else
  {
    if ((exit_code = client()) != 0)
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
