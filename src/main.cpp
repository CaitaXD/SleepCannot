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

#define STR_IMPLEMENTATION
#include "../headers/DataStructures/str.h"
#undef STR_IMPLEMENTATION

#define SOCKET_IMPLEMENTATION
#include "../headers/Sockets.h"
#undef SOCKET_IMPLEMENTATION

#define COMMANDS_IMPLEMENTATION
#include "../headers/commands.h"
#undef COMMANDS_IMPLEMENTATION

#define DISCOVERY_SERVICE_IMPLEMENTATION
#include "../headers/discovery_service.h"
#undef DISCOVERY_SERVICE_IMPLEMENTATION

#include "../headers/management.hpp"
#include "../headers/tcp.hpp"
#include <signal.h>


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
  signal(SIGPIPE, SIG_IGN);
  printf("Server Side\n\n");
  help_msg(NULL);
  DiscoveryService::start_server(port);
  std::vector<MachineEndpoint> clients = {};
  Socket s = socket_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  // Control reading and writting to manager table
  ParticipantTable participants;
  mutex_data_t mutex_data = {std::mutex(), std::condition_variable(), false, 1, {0}};
  std::thread table_readers[1];
  std::thread table_writers[1];
  // Update management table display thread
  table_readers[0] = std::thread(print_management_table, std::ref(participants), std::ref(mutex_data), std::ref(mutex_data.read_count[0]));
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

    MachineEndpoint ep = {};
    if (DiscoveryService::discovered_endpoints.dequeue(ep))
    {
      if (std::find_if(clients.begin(), clients.end(), [&ep](EndPoint other) { return epcmp_inaddr(&ep, &other); }) == clients.end())
      {
      	// Adds new client to the management table
        char* ip = inet_ntoa(((struct sockaddr_in *)&ep.addr)->sin_addr);
        participant_t p = {ep.hostname, ep.mac, ip, true};
        table_writers[0] = std::thread (add_participant, std::ref(participants), std::ref(p), std::ref(mutex_data));
        clients.push_back(ep);
      }
    }

    int next_port = port + 1;
    for(auto [hostname, participant] : participants) {
      if(participant.status) {
          TCP socket = {};
          socket.socket();
          std::cout << "Binding to port " << next_port << std::endl;
          int r = socket.bind(next_port);
          if (r < 0) {
            goto finally;
          }
          std::cout << "Listening" << std::endl;
          r = socket.listen();
          if (r < 0) {
            goto finally;
          }
          EndPoint sep = {};
          r = socket.accept(sep);
          if (r < 0) {
            goto finally;
          }
          std::cout << "Accepted connection" << std::endl;  
          wake_on_lan(socket.clientSocket, participants, hostname, mutex_data);
          socket.close();
      }
    }
  }
finally:
  table_readers[0].join();
  DiscoveryService::stop();
  s.error |= close(s.fd);
  return s.error;
}

int client(int port)
{
  printf("Client Side\n\n");
  DiscoveryService::start_client(port);
  TCP socket = {};
  bool connected = false;
  while (1)
  {
    if (key_hit())
    {
      char buffer[MAXLINE];
      if (fgets(buffer, MAXLINE, stdin) != NULL)
      {
        Str exit_cmd = STR("exit");
        if (str_starts_with(str_take(STR(buffer), strlen(buffer)), exit_cmd))
        {
          break;
        }
      }
    }

    MachineEndpoint ep = {};
    if (!connected && DiscoveryService::discovered_endpoints.dequeue(ep))
    {
      ep = ep.with_port(ep.get_port() + 1);
      std::cout << "My Server Endpoint: " << ep.to_string() << std::endl;
      if(socket.socket() < 0) {
        goto finally;
      }
      if(socket.connect(ip_endpoint(INADDR_ANY, port + 1)) < 0) {
        goto finally;
      }
      connected = true;
    }
    if (!connected) {
      continue;
    }
    std::string cmd;
    int r = socket.recv(cmd);
    std::cout << "Read " << r << " Received command: " << cmd << std::endl;
    if (r < 0)
    {
      goto finally;
    }
    if (cmd == "exit")
    {
      std::cout << "Exiting..." << std::endl;
      goto finally;
    }
    msleep(1000);
  }
finally:
  socket.close();
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
