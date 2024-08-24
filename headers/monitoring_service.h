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
#include <sys/ioctl.h>
#include <net/if.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include <sstream>
#include <string>
#include <pthread.h>
#include <fcntl.h>
#include <poll.h>
#include "macros.h"
#include "Net/Net.hpp"
#include "management.hpp"
#include "node.hpp"
#include <assert.h>
#include "serialization.hpp"
#include <span>

#define MANAGER_TIMEOUT 5
static const char MSG_BEGIN_TABLE[] = "BEGIN TABLE";
static const char MSG_ACK[] = "ACK";
static const char MSG_ELECTION[] = "ELECTION";
static const char MSG_BACK_DOWN[] = "BACK_DOWN";
static const char MSG_OBEY[] = "OBEY";
void election_state_machine_syncronized(Node *node, ParticipantTable &participants);

class Node;
struct MonitoringService
{
public:
  // Constructors
  MonitoringService(Node &node);
  ~MonitoringService();

public:
  // Methods
  void start();
  void stop();
  void mark_as_deleted(const string &host);

private:
  // Private fields
  bool running = {};
  Node *node = {};
  pthread_t _thread = {};

private:
  // Private methods
  void read_table(const string &buffer);
  void read_participant(Peer &recieved_participant);
  void send_msg_if(const string &msg, std::function<bool(Peer &)> predicate);
  void read_peer(int fd, Peer &manager);
};

#ifdef MONITORING_SERVICE_IMPLEMENTATION

struct MonitoringService *monitoring_service_create(class Node *node)
{
  MonitoringService *ms = (MonitoringService *)malloc(sizeof(MonitoringService));
  *ms = MonitoringService{*node};
  return ms;
}

void monitoring_service_start(struct MonitoringService *ms)
{
  ms->start();
}

void monitoring_service_stop(struct MonitoringService *ms)
{
  ms->stop();
}

void monitoring_service_mark_as_deleted(struct MonitoringService *ms, const string &host)
{
  ms->mark_as_deleted(host);
}

MonitoringService::MonitoringService(Node &node) : node(std::addressof(node)) {}
MonitoringService::~MonitoringService() { stop(); }

void MonitoringService::start()
{
  const auto monitoring_function = [](void *data) -> void *
  {
    time_t loop_epoch = time(NULL);
    MonitoringService *self = (MonitoringService *)data;
    Node *node = self->node;
    ParticipantTable &participants = node->participants;
    time_t client_timeout = 3;
    const auto is_senior_peer = [&](Peer &peer) -> bool
    {
      return peer.id > (node->get_info().id);
    };
    const auto is_junior_peer = [&](Peer &peer) -> bool
    {
      return peer.id < (node->get_info().id);
    };
    const auto is_other_peer = [&](Peer &peer) -> bool
    {
      return peer.id != (node->get_info().id);
    };

    (void)is_senior_peer;
    (void)is_junior_peer;
    (void)is_other_peer;

    while (self->running)
    {
      time(&loop_epoch);

      // Write lock
      {
        participants.write_lock();
        node->remove_dead_peers();
        node->accept_peers();
        node->connect_peers();
      }
      // Read lock
      {
        participants.read_lock();
        node->enqueue_messages(POLLIN, 0);
      }

      Message message = {};
      while (node->message_queue.dequeue(message))
      {
        string payload = message.payload;
        Peer peer = message.sender;
        time_t timestamp = message.timestamp;
        if ((loop_epoch - timestamp) > MANAGER_TIMEOUT)
        {
          continue;
        }

        size_t start_msg_begin_table = payload.find(MSG_BEGIN_TABLE);
        size_t start_msg_election = payload.find(MSG_ELECTION);
        size_t start_msg_back_down = payload.find(MSG_BACK_DOWN);
        size_t start_msg_obey = payload.find(MSG_OBEY);
        size_t start_msg_ack = payload.find(MSG_ACK);

        (void)start_msg_ack;
        // Write lock
        {
          participants.write_lock();
          node->get_info().last_conection_timestamp = timestamp;
          participants[peer.machine.hostname].last_conection_timestamp = std::max(timestamp, loop_epoch);
        }

        if (start_msg_begin_table != string::npos)
        {
          string table = payload.substr(start_msg_begin_table);
          self->read_table(table);

          auto &manager = participants.find_id_blocking(node->manager_id); // Manager might have changed in the read_table
          assert(manager.client_socket != nullptr);
          auto &manager_socket = *manager.client_socket;
          int r = manager_socket.send(MSG_ACK);
          if (r < 0)
          {
            std::error_printf(manager_socket.lasterrno, "Error sending ACK to %s", manager.machine.hostname.c_str());
          }
          if ((manager.id < node->get_info().id) && (node->election_state == ElectionState::NoElection))
          {
            LOGF("Manager is a puny weakling. Id %d starting election", manager.id);
            node->election_state = ElectionState::Running;
            time(&node->election_start_time);
            node->highest_id_in_election = std::max(node->highest_id_in_election, manager.id);
            node->send_to_peers(MSG_ELECTION, is_senior_peer);
            node->send_to_peers(MSG_BACK_DOWN, is_junior_peer);
          }
        }
        if (start_msg_election != string::npos)
        {
          LOGF("Received ELECTION from %d", peer.id);
          if (node->election_state == ElectionState::NoElection) // Read lock
          {
            LOGF("Election started");
            participants.read_lock();
            node->election_state = ElectionState::Running;
            time(&node->election_start_time);
            node->highest_id_in_election = std::max(node->highest_id_in_election, peer.id);
            node->send_to_peers(MSG_ELECTION, is_senior_peer);
            node->send_to_peers(MSG_BACK_DOWN, is_junior_peer);
          }
        }
        if (start_msg_back_down != string::npos)
        {
          LOGF("Received BACK_DOWN from %d", peer.id);
          if (node->election_state == ElectionState::Running)
          {
            node->election_state = ElectionState::Overruled;
          }
        }
        if (start_msg_obey != string::npos) // Write lock
        {
          LOGF("Received OBEY from %d", peer.id);
          participants.write_lock();
          if (peer.id < node->get_info().id && node->election_state)
          {
            LOGF("Restarting election peer %d is junior", peer.id);
            node->election_state = ElectionState::Running;
            time(&node->election_start_time);
            node->highest_id_in_election = std::max(node->highest_id_in_election, peer.id);
            node->send_to_peers(MSG_ELECTION, is_senior_peer);
            node->send_to_peers(MSG_BACK_DOWN, is_junior_peer);
          }
          else
          {
            LOGF("Peer %d is senior, changing manager", peer.id);
            node->change_manager(peer.id);
            node->election_start_time = 0;
            node->election_state = ElectionState::NoElection;
            node->highest_id_in_election = -1;
          }
        }
      }

      if (node->is_manager())
      {
        for (auto &[host, peer] : node->participants)
        {
          // Write lock
          {
            participants.write_lock();
            assert(peer.client_socket != nullptr);
            auto &sock = *peer.client_socket;

            if (sock.is_open() && !node->my_info(peer))
            {
              bool writing_will_not_block = (sock.poll(POLLOUT, 0).revents & POLLOUT) != 0;
              if (writing_will_not_block)
              {
                string payload = serialize_table(participants);
                sock.send(payload);
              }
            }
            bool timed_out = (loop_epoch - peer.last_conection_timestamp) > client_timeout;
            participants.update_status(host, !timed_out);
          }
        }
      }
      election_state_machine_syncronized(node, participants);
    }

    return NULL;
  };

  if (running)
    return;
  running = true;
  pthread_create(&_thread, NULL, monitoring_function, this);
}

// Syncronized here means that the calle locks the table and the caller should not lock the table when calling this function
void election_state_machine_syncronized(Node *node, ParticipantTable &participants)
{
  const auto is_senior_peer = [&](Peer &peer) -> bool
  {
    return peer.id > (node->get_info().id);
  };
  const auto is_junior_peer = [&](Peer &peer) -> bool
  {
    return peer.id < (node->get_info().id);
  };
  const auto is_other_peer = [&](Peer &peer) -> bool
  {
    return peer.id != (node->get_info().id);
  };

  time_t election_running_timeout = 10;
  time_t epoch = time(NULL);

  switch (node->election_state)
  {
  case ElectionState::NoElection:
  {
    //  Read lock
    {
      participants.read_lock();
      auto opt_manager = participants.find_manager();
      if (!opt_manager.has_value())
      {
        break;
      }
      auto &manager = **opt_manager;
      bool manager_timed_out = (epoch - manager.last_conection_timestamp) > MANAGER_TIMEOUT;
      if (manager_timed_out)
      {
        LOGF("Manager timed out elapsed %ld", epoch - manager.last_conection_timestamp);
        node->election_state = ElectionState::Running;
        node->election_start_time = epoch;
        auto &info = node->get_info();
        node->highest_id_in_election = std::max(node->highest_id_in_election, info.id);
        node->send_to_peers(MSG_ELECTION, is_senior_peer);
        node->send_to_peers(MSG_BACK_DOWN, is_junior_peer);
      }
    }
    break;
  }
  case ElectionState::Running:
  {
    if ((epoch - node->election_start_time) > election_running_timeout)
    {
      // Write lock
      {
        participants.write_lock();
        LOGF("Election Ended checking winner");
        auto &node_info = node->get_info();
        if (node->highest_id_in_election <= node_info.id)
        {
          LOGF("I WIN %d", node->get_info().id);
          node->election_state = ElectionState::NoElection;
          node->election_start_time = 0;
          node->highest_id_in_election = -1;
          node->send_to_peers(MSG_OBEY, is_other_peer);
          node->change_manager(node_info.id);
        }
        else
          goto CASE_END_ELECTION;
      }
    }
    break;
  }
  case ElectionState::Overruled:
  {
    if ((epoch - node->election_start_time) > election_running_timeout)
    {
    CASE_END_ELECTION:
      LOGF("Back to work");
      node->election_state = ElectionState::NoElection;
      node->election_start_time = 0;
      node->highest_id_in_election = -1;
    }
    break;
  }
  }
}

void MonitoringService::stop()
{
  running = false;
  pthread_join(_thread, NULL);
}

void MonitoringService::mark_as_deleted(const string &host)
{
  (void)host;
}

void MonitoringService::read_table(const string &buffer)
{
  const char delimiter = '\t';
  std::vector<string> arr = string_split(buffer, delimiter);
  ptrdiff_t end = find(arr.begin(), arr.end(), "END TABLE") - arr.begin();
  std::span<string> parts = std::span<string>(arr).subspan(2, end - 2);
  for (size_t i = 0; i < parts.size(); i++)
  {
    auto optional_participant = deserialize_participant(parts);
    if (!optional_participant.has_value())
      continue;

    read_participant(*optional_participant);
  }
}

void MonitoringService::read_participant(Peer &recieved_participant)
{
  auto &node_info = node->get_info();
  ParticipantTable &participants = node->participants;
  StringEqComparerIgnoreCase string_equals;
  auto recieved_machine = recieved_participant.machine;
  int recieved_id = recieved_participant.id;
  bool recieved_status = recieved_participant.status;
  bool recieved_is_manager = recieved_participant.is_manager;
  time_t received_time_last = recieved_participant.last_conection_timestamp;

  if (recieved_is_manager)
  {
    node->manager_id = recieved_id;
  }

  node_info.is_manager = recieved_is_manager && recieved_id == node_info.id;
  if (string_equals(recieved_machine.hostname, node_info.machine.hostname))
  {
    node_info.id = recieved_id;
    node_info.status = recieved_status;
    node_info.last_conection_timestamp = received_time_last;
    node_info.is_manager = recieved_is_manager;
    node_info.machine = recieved_machine;
    assert(node_info.client_socket != nullptr);
  }
  else
  {
    auto it = participants.find(recieved_machine.hostname);
    if (it == participants.end())
    {
      LOGF("Inserted %s", recieved_machine.hostname.c_str());
      Peer p;
      p.last_conection_timestamp = received_time_last;
      p.client_socket = std::make_unique<Socket>();
      p.id = recieved_id;
      p.is_manager = recieved_is_manager;
      p.status = recieved_status;
      p.machine = recieved_machine;
      participants.add(p);
      assert(p.client_socket != nullptr);
    }
    else
    {
      auto &[host, p] = *it;
      p.last_conection_timestamp = received_time_last;
      assert(p.client_socket != nullptr);
    }
  }
}

#endif // MONITORING_SERVICE_IMPLEMENTATION
#endif // MONITORING_SERVICE_H_
