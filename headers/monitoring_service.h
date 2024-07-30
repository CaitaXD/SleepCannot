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

private:
  int prepare_conections(std::vector<FileDescriptor *> &conections, time_t timeout);
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
      PRINT_ERROR("start_server");
      return NULL;
    }

    std::string packet;
    std::vector<FileDescriptor*> conections = std::vector<FileDescriptor*>();
    while(ms->running)
    { 
      conections.clear();  
      ms->participants->lock();
      auto size = ms->participants->map.size();
      if (size == 0) {
        ms->participants->unlock();
        continue;
      }
      
      result |= ms->prepare_conections(conections, 3);
      if (result < 0) {
        PRINT_ERROR("prepare_conections");
        ms->participants->unlock();
        result = 0;
        continue;
      }

      ms->participants->unlock();
      msleep(300); // Let other threads get the GODDAMN MUTEX

      std::vector<string> to_remove;

      auto polls = FileDescriptor::poll(conections, POLLIN|POLLHUP|POLLOUT, 1000);
      ms->participants->sync_root.lock();
      for (auto &poll : polls) {
        int file_descriptor = poll.fd;
        participant_t *participant;
        string host;
        if (!ms->participants->try_get_by_file_descriptor(file_descriptor, host, participant)) {
          continue;
        }

        char buffer[1024];
        int read = recv(file_descriptor, ARRAY_POSTFIXLEN(buffer), 0);
        
        result |= read;
        if (errno == EPIPE) {
          to_remove.push_back(host);
          ms->participants->dirty = true;
          result = 0;
          continue;
        }
        if (result < 0) {
          PRINT_ERROR("recv");
          result = 0;
          close(file_descriptor);
          continue;
        }

        if (result == 0) {
          std::cerr << "No Data avaliable to read" << std::endl;
          continue;
        }

        if (string_equals(string(buffer, read), "exit")) {
          participant->socket->close();
          to_remove.push_back(host);
          ms->participants->dirty = true;
        }

        participant->last_conection_timestamp = time(NULL);
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

int MonitoringService::prepare_conections(std::vector<FileDescriptor *> &connections, time_t timeout)
{
  int result = 0;
  for (auto &[host, participant] : participants->map)
  {
    if ((participant.last_conection_timestamp + timeout) < time(NULL))
    {
      participants->update_status(host, false);
    }
    else
    {
      participants->update_status(host, true);
    }
    Socket *s = participant.socket.get();
    if (s->file_descriptor == -1)
    {
      IpEndpoint client_endpoint;
      *s = tcp_socket.accept(client_endpoint);
      if (s->file_descriptor == -1)
      {
        participants->update_status(host, false);
        close(s->file_descriptor);
        s->file_descriptor = -1;
        continue;
      }
      result |= s->file_descriptor;
    }
    connections.push_back(s);
    result |= s->send("probe from server");
    if (result < 0)
    {
      close(s->file_descriptor);
      s->file_descriptor = -1;
      return result;
    }
  }
  return result;
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
        PRINT_ERROR("connect");
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
        PRINT_ERROR("recv");
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
          PRINT_ERROR("recv");
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
