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

#define COMMANDS_IMPLEMENTATION
#include "../headers/commands.h"
#undef COMMANDS_IMPLEMENTATION

#define DISCOVERY_SERVICE_IMPLEMENTATION
#include "../headers/discovery_service.h"
#undef DISCOVERY_SERVICE_IMPLEMENTATION

#include "../headers/management.hpp"
#include "../headers/Socket.hpp"
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
  int monitoring_port = port + 1;
  printf("Manager\n");
  help_msg_server();
  DiscoveryService discovery_service{port};
  discovery_service.start_server();
  std::vector<MachineEndpoint> clients = {};
  Socket s{};
  s.open(AdressFamily::InterNetwork, SocketType::Datagram, SocketProtocol::UDP);
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
      command_exec(participants, mutex_data.mutex);
    }

    MachineEndpoint ep = {INADDR_ANY, port};
    if (discovery_service.endpoints.dequeue(ep))
    {
      if (std::find_if(clients.begin(), clients.end(), [&](IpEndPoint other)
                       { return ep == other; }) == clients.end())
      {
        // Adds new client to the management table
        char *ip = inet_ntoa(((struct sockaddr_in *)&ep.addr)->sin_addr);
        participant_t p = {ep.hostname, ep.mac, ip, true};
        table_writers[0] = std::thread(add_participant, std::ref(participants), std::ref(p), std::ref(mutex_data));
        clients.push_back(ep);
      }
    }

    std::unique_lock<std::mutex> lock(mutex_data.mutex);
    std::string packet{};
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    for (auto [hostname, participant] : participants)
    {
      if (participant.status)
      {
        packet.clear();
        IpEndPoint sep{};
        Socket socket{};
        Socket cli{};
        socket.open(AdressFamily::InterNetwork, SocketType::Stream);
        socket.set_option(SO_REUSEADDR, 1);
        socket.set_option(SO_RCVTIMEO, &timeout);
        socket.set_option(SO_SNDTIMEO, &timeout);
        int r;
        int max_tries = 100;
        do
        {
          r = socket.bind(monitoring_port);
          msleep(100);
        } while (errno == EADDRINUSE && max_tries--);
        if (r < 0)
        {
          perror("bind");
          goto finally_1;
        }
        r = socket.listen();
        if (r < 0)
        {
          goto finally_1;
        }
        r = socket.accept(sep);
        if (r < 0)
        {
          goto finally_1;
        }
        cli.sockfd = socket.client_socket;
        cli.send("probe");
        // r = cli.recv(&packet);
        // if (errno == ETIMEDOUT)
        // {
        //   participant.status = false;
        // }
        // else if (packet == "probe")
        // {
        //   participant.status = true;
        // }
        // else if (r < 0){
        //   perror("recv");
        // }
      finally_1:
        socket.close();
      }
    }
  }
  table_readers[0].join();
  discovery_service.stop();
  s.close();
  return 0;
}

int client(int port)
{
  DiscoveryService discovery_service{port};
  printf("Participant\n");
  help_msg_client();
  discovery_service.start_client();
  Socket socket = {};
  bool connected = false;
  while (1)
  {
    if (key_hit())
    {
      char buffer[MAXLINE];
      if (fgets(buffer, MAXLINE, stdin) != NULL)
      {
        std::string_view buffer_view = std::string_view(buffer, strlen(buffer));
        if (buffer_view.rfind("exit"))
        {
          break;
        }
      }
    }

    MachineEndpoint ep = {};
    std::string cmd;
    int r = -1;
    if (!connected && discovery_service.endpoints.peek(ep))
    {
      std::cout << "Manager Endpoint: " << ep.to_string() << std::endl;
      int max_tries = 100;
      int monitoring_port = ep.get_port() + 1;
      ep = ep.with_port(monitoring_port);
      r = socket.open(AdressFamily::InterNetwork, SocketType::Stream);
      if (r < 0)
      {
        goto finally_1;
      }
      do
      {
        r = socket.connect(ep);
        msleep(100);
      } while ((errno == EADDRINUSE || errno == ECONNREFUSED) && max_tries--);
      if (r < 0)
      {
        perror("connect");
        goto finally_1;
      }
      connected = true;
    }
    if (!connected)
    {
      continue;
    }
    r = socket.recv(&cmd);
    if (r <= 0)
    {
      goto finally_1;
    }
    if (cmd == "exit")
    {
      std::cout << "Exiting..." << std::endl;
      goto finally_1;
    }
    else if (cmd == "probe")
    {
      std::cout << "Probing..." << std::endl;
      socket.send("probe");
      socket.recv(&cmd);
    }
    msleep(1000);
  finally_1:
    socket.close();
    connected = false;
  }
  discovery_service.stop();
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
