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

struct MonitoringService
{
  bool running;
  int port;
  pthread_t thread;
  IpEndpoint server_machine;
  ParticipantTable *participants;
  Socket tcp_socket;

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
    StringEqComparerIgnoreCase string_equals;
    MonitoringService *ms = (MonitoringService *)data;
    timeval timeout = {
        .tv_sec = 1,
        .tv_usec = 0
      };
    int result = ms->tcp_socket.open(SocketType::Stream, SocketProtocol::TCP);
    result |= ms->tcp_socket.set_option(SO_REUSEADDR, 1);
    result |= ms->tcp_socket.set_option(SO_RCVTIMEO, &timeout);
    result |= ms->tcp_socket.set_option(SO_SNDTIMEO, &timeout);
    result |= ms->tcp_socket.set_option(SO_REUSEADDR, 1);
    result |= ms->tcp_socket.bind(ms->port); 
    result |= ms->tcp_socket.listen();
    if(result < 0)
    {
      perrorcode("start_server");
      return NULL;
    }

    std::string packet;
    while(ms->running)
    {   
      ms->participants->lock();
      
      auto size = ms->participants->map.size();
      if (size == 0) {
        ms->participants->unlock();
        continue;
      }
      
      std::vector<FileDescriptor*> to_poll = std::vector<FileDescriptor*>(size);
      time_t unix_epoch_now = time(NULL);
      time_t time_before_wake = 5;
      size_t to_poll_idx = 0;
      for (auto &[host, participant] : ms->participants->map) {
        if (participant.last_conection_timestamp + time_before_wake < unix_epoch_now) {
          ms->participants->update_status(host, false);
        }
        else {
          ms->participants->update_status(host, true);
        }
        if (participant.socket->file_descriptor == -1) {
          IpEndpoint client_endpoint;
          Socket client_socket = ms->tcp_socket.accept(client_endpoint);
          if (client_socket.file_descriptor == -1) {
            ms->participants->update_status(host, false);
            continue;
          }
          result |= client_socket.file_descriptor;
          *participant.socket = std::move(client_socket);
          to_poll[to_poll_idx++] = participant.socket.get();
        }
        result = participant.socket->send("probe from server");
      }

      ms->participants->unlock();
      msleep(300); // Let other threads get the GODDAMN MUTEX

      std::vector<string> to_remove;
      auto polls = FileDescriptor::poll(to_poll, POLLIN|POLLPRI|POLLOUT, 1000);
      
      ms->participants->sync_root.lock();
      for (auto &poll : polls) {
        int file_descriptor = poll.fd;
        auto begin = ms->participants->map.begin();
        auto end = ms->participants->map.end();
        auto it = std::find_if(begin, end, [&file_descriptor](auto &p){ 
          return p.second.socket->file_descriptor == file_descriptor;
        });
        if (it == end) {
          continue;
        }

        auto &[host, participant] = *it;
        char buffer[1024];
        int read = recv(file_descriptor, ARRAY_POSTFIXLEN(buffer), 0);
        
        result |= read;
        if (errno == EPIPE) {
          to_remove.push_back(host);
          ms->participants->dirty = true;
          continue;
        }

        if (result < 0) {
          perrorcode("recv");
          continue;
        }

        if (result == 0) {
          std::cerr << "No Data avaliable to read" << std::endl;
          continue;
        }

        if (string_equals(string(buffer, read), "exit")) {
          to_remove.push_back(host);
          ms->participants->dirty = true;
        }

        participant.last_conection_timestamp = unix_epoch_now;
        participant.socket->close();
      }

      for (auto host : to_remove) {
        ms->participants->remove(host);
      }
      
      ms->participants->unlock();
      msleep(300); // Let other threads get the GODDAMN MUTEX
    }
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
    Socket &client_socket = ms->tcp_socket;
    int result = client_socket.open(SocketType::Stream, SocketProtocol::TCP);
    result |= client_socket.connect(ms->server_machine);
    
    std::string cmd;
    while (ms->running)
    {
      if (result < 0)
      {
        perror("connect");
        return NULL;
      }
      result = client_socket.recv(&cmd);
      if (result == 0) {
        client_socket.close();
        result = client_socket.open(SocketType::Stream, SocketProtocol::TCP);
        result |= client_socket.connect(ms->server_machine);
        continue;
      }
      else if (result < 0) {
        perrorcode("recv");
        continue;
      }
      if (cmd == "probe from server")
      {
        msleep(1000);
        result |= client_socket.send("probe from client");
        if (result == 0){
          continue;
        }
        else if (result < 0) {
          perrorcode("recv");
        }
      }
      else if (cmd == "exit") {
        ms->running = false;
        return NULL;
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
