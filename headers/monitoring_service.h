/*
  This service is used to monitor the network for new participants
  It TCP to exchange messages with the all the participants in the network
  Once a participant connects it its file descriptor is added to the polling list
  If a client doesnt respond for a while it is considered as sleeping if a client sends the exit command or exits via SIG_INT it gets removed from the table
*/
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
#include "node.hpp"

class Node; // forward declaration

struct MonitoringService
{
  bool running = {};
  int port = {};
  pthread_t thread = {};
  Node *node = {};

  MonitoringService(Node &node)
  {
    this->node = std::addressof(node);
  }

  ~MonitoringService()
  {
    stop();
  }

  void start_service();
  void stop();

private:
  std::vector<FileDescriptor *> file_descriptors = {};
  std::vector<string> to_remove = {};
  void monitor_peers();
  void update_peers_status(time_t timeout = 5);
  void collect_file_descriptors();
};

struct MonitoringService *monitoring_service(class Node* node) {
  return new MonitoringService{*node};
}


#ifdef MONITORING_SERVICE_IMPLEMENTATION

void MonitoringService::start_service()
{
  if (running)
    return;
  running = true;

  pthread_create(&thread, NULL, [](void *data) -> void *
                 {
    //StringEqComparerIgnoreCase string_equals;
    MonitoringService *m = (MonitoringService *)data;
    Node &node = *m->node;
    ParticipantTable &participants = node.participants;
    while(m->running) {
      participants.lock();
      {
        auto size = participants.map.size();
        if (size == 0) {
          participants.unlock();
          continue;
        }
        m->monitor_peers();
      }
      participants.unlock();
      msleep(300); // Let other threads get the GODDAMN MUTEX
    }
    m->running = false;
    return NULL; }, this);
}

void MonitoringService::monitor_peers()
{
  StringEqComparerIgnoreCase string_equals;
  ParticipantTable &participants = node->participants;
  file_descriptors.clear();
  to_remove.clear();
  collect_file_descriptors();
  update_peers_status(1);

  auto poll_result = FileDescriptor::poll(file_descriptors, POLLIN, 5000);
  for (auto &poll : poll_result)
  {
    Socket sock{poll.fd};
    sock.keep_alive = true;

    auto optional_peer = participants.find_by_socket(sock);
    if (!optional_peer.has_value())
      continue;

    auto &[perr_name, peer_refrence] = optional_peer.value();
    auto &peer = peer_refrence.get();

    string buffer(1024, '\0');
    int read = sock.recv(&buffer);
    if (errno == EPIPE)
    {
      perrorcode("recv");
      to_remove.push_back(perr_name);
      participants.dirty = true;
      continue;
    }

    if (read < 0)
    {
      perrorcode("recv");
      continue;
    }

    if (node->is_manager())
    {
      if (string_equals(buffer, "exit"))
      {
        to_remove.push_back(perr_name);
        participants.dirty = true;
      }
      read = peer.socket->send(server_msg);
    }
    else
    {
      read = peer.socket->send(client_msg);
    }

    if (errno == EPIPE)
    {
      to_remove.push_back(perr_name);
      participants.dirty = true;
      continue;
    }

    if (read < 0)
    {
      perrorcode("send");
      continue;
    }
    peer.last_conection_timestamp = time(NULL);
  }

  for (auto host : to_remove)
  {
    participants.remove(host);
  }
}

void MonitoringService::update_peers_status(time_t timeout)
{
  ParticipantTable &participants = node->participants;
  time_t epoch_now = time(NULL);
  if (node->is_manager())
  {
    for (auto &[host, participant] : participants.map)
    {
      if (node->my_self(participant)) continue;
      auto elapsed = participant.last_conection_timestamp - epoch_now;
      bool peer_awake = elapsed + timeout >= 0;
      participants.update_status(host, peer_awake);\
      std::printf("Host: %s, status: %s\n", host.c_str(), participant.status ? "awake" : "sleeping");
    }
  }
}

void MonitoringService::collect_file_descriptors()
{
  ParticipantTable &participants = node->participants;
  for (auto &[host, participant] : participants.map)
  {
    bool mySelf = participant.id == node->info.id;
    if (node->is_manager() && !mySelf)
    {
      if (participant.socket->file_descriptor > -1)
      {

        file_descriptors.push_back(participant.socket.get());
        int result = participant.socket->send(server_msg);

        if (errno == EPIPE)
        {
          to_remove.push_back(host);
          participants.dirty = true;
          continue;
        }

        if (result < 0)
        {
          perrorcode("send");
          continue;
        }
      }
    }
    else if (participant.socket->file_descriptor > -1)
    {
      file_descriptors.push_back(participant.socket.get());
    }
  }
}

void MonitoringService::stop()
{
  running = false;
  pthread_join(thread, NULL);
}

#endif // MONITORING_SERVICE_IMPLEMENTATION
#endif // MONITORING_SERVICE_H_
