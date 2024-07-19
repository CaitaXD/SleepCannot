#ifndef MONITORING_SERVICE_H_
#define MONITORING_SERVICE_H_

#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
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
#include <fcntl.h>
#include <poll.h>
#include "macros.h"
#include "Net/Net.hpp"
#include "management.hpp"

class MonitoringService
{
public:
  bool running;
  const int port;

private:
  pthread_t thread;
  IpEndpoint server_machine;
  ParticipantTable *participants;

public:
  MonitoringService(int port) : running(false), port(port) {}
  ~MonitoringService()
  {
    stop();
  }
  void start_server(ParticipantTable &participants);
  void start_client(const IpEndpoint &server_machine);
  void stop();
};

#endif // MONITORING_SERVICE_H_

#ifdef MONITORING_SERVICE_IMPLEMENTATION

void MonitoringService::start_server(ParticipantTable &participants)
{
  if (running)
  {
    return;
  }
  running = true;
  this->participants = std::addressof(participants);
  pthread_create(&thread, NULL, [](void *data) -> void *
                 {
    MonitoringService *ms = (MonitoringService *)data;
    Socket server_socket{};
    int result = server_socket.open(SocketType::Stream, SocketProtocol::TCP);
    result |= server_socket.bind(ms->port); 
    result |= server_socket.set_option(SO_REUSEADDR, 1);
    result |= server_socket.listen();
    if(result < 0)
    {
      perrorcode("start_server");
      return NULL;
    }

    std::string packet;
    while(ms->running)
    {   
      ms->participants->lock();
      for (auto [host, participant] : ms->participants->map) {
        if (participant.socket->file_descriptor == -1) {
          IpEndpoint client_endpoint;
          Socket client_socket = server_socket.accept(client_endpoint);
          result |= client_socket.file_descriptor;
          *participant.socket = std::move(client_socket);
        }
        result = participant.socket->send("probe from server");
      }
      msleep(1001);
      std::vector<string> to_remove;

      // int num_events;
      // std::vector<Socket*> to_poll = std::vector<Socket*>(ms->participants->map.size());
      // for (auto [host, participant] : ms->participants->map) {
      //   to_poll.emplace_back(participant.socket.get());
      // }
      // std::vector<pollfd> poll_result = Socket::poll(to_poll, num_events, 5000);
      // for (auto &poll : poll_result) {
      //   if (poll.revents & POLLIN) {
      //     Socket client_socket = Socket{poll.fd};
      //     client_socket.keep_alive = true;
      //     std::string cmd;
      //     result = client_socket.recv(&cmd);
      //     if (result <= 0) {
      //       auto begin = ms->participants->map.begin();
      //       auto end = ms->participants->map.end();
      //       auto neeedle = std::find_if(begin, end, [&client_socket](const auto &pair) {
      //         return pair.second.socket->file_descriptor == client_socket.file_descriptor;
      //       });
      //       if (neeedle != end) {
      //         to_remove.push_back(neeedle->first);
      //       }
      //       perrorcode("recv");
      //       continue;
      //     }
      //   }
      // }

      for (auto host : to_remove) {
        ms->participants->remove(host);
      }
      ms->participants->unlock();
    }
    std::cout << "Monitoring Service Stopped" << std::endl;
    ms->running = false;
    return NULL; }, this);
}

void MonitoringService::start_client(const IpEndpoint &server_machine)
{
  if (running)
  {
    return;
  }
  running = true;
  this->server_machine = server_machine.with_port(port);
  pthread_create(&thread, NULL, [](void *data) -> void *
                 {
    MonitoringService *ms = (MonitoringService *)data;
    int result = 0;
    Socket client_socket{};
    result = client_socket.open(SocketType::Stream, SocketProtocol::TCP);
    result |= client_socket.connect(ms->server_machine);
    if (result < 0)
    {
      perror("connect");
      return NULL;
    }
    std::string cmd;
    
    while (ms->running)
    {
      result = client_socket.recv(&cmd);
      if (result == 0) {
        continue;
      }
      else if (result < 0) {
        perrorcode("recv");
        continue;
      }
      std::cout << "Received: " << cmd << " from " << ms->server_machine.to_string() << std::endl;
      if (cmd == "probe from server")
      {
        msleep(1001);
        result |= client_socket.send("probe from client");
        if (result == 0){
          continue;
        }
        else if (result < 0) {
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