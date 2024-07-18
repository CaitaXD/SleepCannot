#ifndef MONITORING_SERVICE_H_
#define MONITORING_SERVICE_H_

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
#include <vector>
#include <pthread.h>
#include "macros.h"
#include "commands.hpp"

class MonitoringService
{
public:
  bool running;
  const int port;

private:
  pthread_t thread;
  MachineEndpoint server_machine;
  ParticipantTable *participants;

public:
  MonitoringService(int port) : port(port) {}
  ~MonitoringService()
  {
    stop();
  }
  void start_server(ParticipantTable &participants);
  void start_client(MachineEndpoint &server_machine);
  void stop();
};

#endif // MONITORING_SERVICE_H_
#ifdef MONITORING_SERVICE_IMPLEMENTATION

void MonitoringService::start_server(ParticipantTable &participants)
{
  running = true;
  this->participants = std::addressof(participants);
  pthread_create(&thread, NULL, [](void *data) -> void *
                 {
    MonitoringService *ms = (MonitoringService *)data;
    Socket server{};
    struct timeval timeout {
      tv_sec : 3,
      tv_usec : 0
    };
    int result = server.open(AdressFamily::InterNetwork, SocketType::Stream, SocketProtocol::TCP);
    result |= server.bind(ms->port);
    result |= server.set_option(SO_REUSEADDR, 1);
    result |= server.set_option(SO_RCVTIMEO, timeout);
    result |= server.set_option(SO_SNDTIMEO, timeout);
      
    if(result < 0)
    {
      perrorcode("start_server");
      return NULL;
    }

    std::string packet{};
    while(1)
    {   
      ms->participants->lock();
      for (auto [host, participant] : ms->participants->map) {
        if (participant.socket == nullptr || participant.socket->sockfd == -1)
        {
          result |= server.listen();
          if (result < 0) {
            perrorcode("listen");
            continue;
          }
          IpEndPoint client_endpoint{};
          Socket client = server.accept(client_endpoint);
          if (client.sockfd < 0) {
            perrorcode("accept");
            continue;
          }
          else {
            std::cout << "Accepted connection from: " << client_endpoint.to_string() << std::endl;
            participant.socket = std::make_shared<Socket>(std::move(client));
          }
          result |= participant.socket->set_option(SO_RCVTIMEO, &timeout);
          result |= participant.socket->set_option(SO_SNDTIMEO, &timeout);
        }
        else {
          Socket &client = *participant.socket;
          std::string cmd;
          result |= client.send("probe");
          std::cout << "Sending probe to " << host << std::endl;
          result |= client.recv(&cmd);
          if (cmd == "probe")
          {
            std::cout << "Received probe from " << host << std::endl;
            participant.status = true;
          }
        }
      }
    }
    ms->running = false;
    return NULL; }, this);
}

void MonitoringService::start_client(MachineEndpoint &server_machine)
{
  running = true;
  this->server_machine = server_machine.with_port(port);
  pthread_create(&thread, NULL, [](void *data) -> void *
                 {
    MonitoringService *ms = (MonitoringService *)data;
    int result;
    Socket socket{};
    while (result < 0) {
      result = socket.open(AdressFamily::InterNetwork, SocketType::Stream, SocketProtocol::TCP);
      result |= socket.connect(ms->server_machine);
      if (result < 0)
      {
        perror("connect");
        continue;
      }
    }
    std::cout << "Monitoring Endpoint: " << ms->server_machine.to_string() << std::endl;
    std::string cmd;
    while (ms->running)
    {
      result |= socket.recv(&cmd);
      if (cmd == "exit")
      {
        std::cout << "Exiting..." << std::endl;
        ms->running = false;
        return NULL;
      }
      else if (cmd == "probe")
      {
        std::cout << "Probing..." << std::endl;
        result |= socket.send("probe");
        if (result < 0) {
          perrorcode("recv");
        }
      }
    }
    ms->running = false;
    return NULL; }, this);
}

void MonitoringService::stop()
{
  running = false;
  pthread_join(thread, NULL);
}

#endif // MONITORING_SERVICE_IMPLEMENTATION