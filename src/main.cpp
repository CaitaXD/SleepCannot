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

int server(int port)
{
  int discovery_port = port;
  int monitoring_port = port + 1;

  printf("Manager\n");
  help_msg_server();
  ParticipantTable participants;
  DiscoveryService discovery_service{discovery_port};
  MonitoringService monitoring_service{monitoring_port};

  discovery_service.start_server();
  monitoring_service.start_server(participants);
  while (1)
  {
    if (key_hit())
    {
      command_exec(participants);
    }

    participants.lock();
    participants.print();
    MachineEndpoint discoveredMachine = {};
    if (discovery_service.endpoints.dequeue(discoveredMachine))
    {
      if (participants.map.find(discoveredMachine.hostname) == participants.map.end())
      {
        participants.add(participant_t{discoveredMachine, true, std::make_shared<Socket>()});
      }
    }
    participants.unlock();
  }
  return 0;
}

int client(int port)
{
  int discovery_port = port;
  int monitoring_port = port + 1;

  std::mutex mutex;
  DiscoveryService discovery_service{discovery_port};
  MonitoringService monitoring_service{monitoring_port};
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
        break;
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
  bool is_server = argc > 1 && !strcmp(argv[1], "manager");
  int port = INITIAL_PORT;

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
