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
#include <span>

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
  void mark_as_deleted(const string &host);

private:
  std::vector<FileDescriptor *> file_descriptors = {};
  void monitor_peers();
  void read_table(const string &buffer);
  string serialize_table();
  string serialize_participant(const participant_t &participant);
  void update_peers_status(time_t timeout = 5);
  void collect_file_descriptors();
  void send_msg_if(const string &msg, std::function<bool(participant_t &)> predicate);
  std::vector<string> to_remove = {};
};

struct MonitoringService *monitoring_service(class Node *node)
{
  MonitoringService *ms = (MonitoringService *)malloc(sizeof(MonitoringService));
  *ms = MonitoringService{*node};
  return ms;
}

void monitoring_service_start(class MonitoringService *ms)
{
  ms->start_service();
}

#ifdef MONITORING_SERVICE_IMPLEMENTATION

std::vector<std::string> splitString(const std::string &input, char delimiter)
{
  std::istringstream stream(input);

  std::string token;

  std::vector<std::string> arr;

  while (std::getline(stream, token, delimiter))
  {
    arr.push_back(token);
  }

  return arr;
}

void MonitoringService::start_service()
{
  LOG("Monitoring service started");
  if (running)
    return;
  running = true;

  pthread_create(&thread, NULL, [](void *data) -> void *
                 {	
    //StringEqComparerIgnoreCase string_equals;
    MonitoringService *m = (MonitoringService *)data;
    //Node &node = *m->node;
    //ParticipantTable &participants = node.participants;
    while(m->running) {
      //participants.lock();
      m->monitor_peers();
      //participants.unlock();
      //rsleep(); // Let other threads get the GODDAMN MUTEX
    }
    m->running = false;
    return NULL; }, this);
}

void MonitoringService::monitor_peers()
{
  const auto other_peers = [&](participant_t &peer) -> bool
  {
    return !node->my_self(peer);
  };
  const auto senior_peers = [&](participant_t &peer) -> bool
  {
    return peer.id > node->info.id;
  };
  const auto junior_peers = [&](participant_t &peer) -> bool
  {
    return peer.id < node->info.id;
  };
  const char MSG_ELECTION[] = "ELECTION";
  const char MSG_OVERRULED[] = "OVERULED";
  const char MSG_OBEY[] = "OBEY";
  const char MSG_ACK[] = "ACK";
  const char MSG_BEGIN_TABLE[] = "BEGIN TABLE";
  ParticipantTable &participants = node->participants;

  collect_file_descriptors();
  if (node->is_manager())
  {
    auto &info = node->info;
    info.last_conection_timestamp = time(NULL);
    participants.map[node->info.machine.hostname] = info;
    update_peers_status(1);
    send_msg_if(serialize_table(), other_peers);
  }
  else
  {
    switch (node->election_state)
    {
    case ElectionState::NoElection:
    {
      auto optional_manager = participants.find_manager();
      if (!optional_manager.has_value())
        return;
      auto &manager = optional_manager.value().second.get();

      manager.socket->send(client_msg);

      auto elapsed_s = time(NULL) - manager.last_conection_timestamp;
      bool manager_timeout = (1000 * elapsed_s) >= 10000;
      if (manager_timeout)
      {
        node->election_start_time = time(NULL);
        LOGF("Manager %d not responding elapsed %d", manager.id, (int)(time(NULL) - manager.last_conection_timestamp));
        node->election_state = ElectionState::Running;
        send_msg_if(MSG_ELECTION, senior_peers);
        send_msg_if(MSG_OVERRULED, junior_peers);
      }
      break;
    }
    case ElectionState::Running:
    {
      auto elapsed_s = time(NULL) - node->election_start_time;
      bool election_timeout = (1000 * elapsed_s) > 10000;
      if (election_timeout)
      {
        LOGF("ELECTION ENDED, MAX ID %d", std::max(node->highest_id_in_election, node->info.id));
        if (node->highest_id_in_election <= node->info.id)
        {
          node->change_manager(node->info.id);
        }
        node->election_state = NoElection;
        node->highest_id_in_election = -1;
      }
      break;
    }
    case ElectionState::Overruled:
    {
      auto elapsed_s = time(NULL) - node->election_start_time;
      bool coordinatrion_timeout = (1000 * elapsed_s) > 10000;
      if (coordinatrion_timeout)
      {
        LOGF("COORDINATION ENDED, MAX ID %d", std::max(node->highest_id_in_election, node->info.id));
        node->election_state = ElectionState::NoElection;
        node->election_start_time = time(NULL);
        node->highest_id_in_election = -1;
      }
      break;
    }
    }
  }

  auto poll_result = FileDescriptor::poll(file_descriptors, POLLIN, 5000);
  for (auto &poll : poll_result)
  {
    if (node->my_fd(poll.fd))
      continue;

    Socket sock{poll.fd};
    sock.keep_alive = true;

    auto optional_peer = participants.find_by_socket(sock);
    if (!optional_peer.has_value())
      continue;

    auto &[perr_host, peer_refrence] = optional_peer.value();
    auto &peer = peer_refrence.get();
    peer.last_conection_timestamp = time(NULL);

    string buffer(1024, '\0');
    int read = sock.recv(&buffer);

    if (read <= 0)
    {
      if (read < 0)
        perrorcode("recv");
      continue;
    }

    size_t idx_EXIT = buffer.find("exit");
    if (idx_EXIT != string::npos)
    {
      LOGF("Exiting from %s", perr_host.c_str());
      sock.close();
      to_remove.push_back(perr_host);
      participants.dirty = true;
      continue;
    }

    size_t idx_ELECTION = buffer.rfind(MSG_ELECTION, 0);
    size_t idx_OVERRULED = buffer.rfind(MSG_OVERRULED, 0);
    size_t idx_OBEY = buffer.rfind(MSG_OBEY, 0);
    size_t idx_BEGIN_TABLE = buffer.rfind(MSG_BEGIN_TABLE, 0);

    if (idx_ELECTION != string::npos)
    {
      if (node->election_state == ElectionState::NoElection)
      {
        LOGF("RECIVED ELECTION FROM %d", peer.id);
        if (node->is_manager())
        {
          int result = peer.socket->send(MSG_OBEY);
          if (result < 0)
          {
            perrorcode("send");
          }
        }
        else
        {
          node->election_start_time = time(NULL);
          node->highest_id_in_election = std::max(node->highest_id_in_election, peer.id);
          send_msg_if(MSG_ELECTION, senior_peers);
          send_msg_if(MSG_OVERRULED, junior_peers);
          node->election_state = ElectionState::Running;
        }
      }
      else
      {
        peer.socket->send(MSG_ACK);
      }
    }

    if (idx_OVERRULED != string::npos)
    {
      if (node->election_state == ElectionState::Overruled)
      {
        peer.socket->send(MSG_ACK);
      }
      else
      {
        if (peer.id < node->info.id)
        {
          LOGF("DEGENERATE CASE: JUNIOR PEER %d CANNOT OVERRULE ME", peer.id);
          peer.socket->send(MSG_OVERRULED); // NO U
        }
        else
        {
          LOGF("OVERRULED BY %d", peer.id);
          peer.socket->send(MSG_ACK);
          node->highest_id_in_election = -1;
          node->election_state = ElectionState::Overruled;
        }
      }
    }

    if (idx_OBEY != string::npos)
    {
      LOGF("RECIVED OBEY FROM ID %d", peer.id);
      if (peer.id < node->info.id)
      {
        LOGF("JUNIOR %d IN CHARGE STARNING NEW ELECTION", peer.id);
        node->election_state = ElectionState::Running;
        send_msg_if("ELECTION", senior_peers);
        send_msg_if("OVERRULED", junior_peers);
      }
      else
      {
        LOGF("OBEYING %d", peer.id);
        peer.socket->send("ACK");
        node->highest_id_in_election = -1;
        node->election_state = ElectionState::NoElection;
        node->change_manager(peer.id);
      }
    }

    if (!node->is_manager())
    {
      FUZZ_DELAY;
      read = peer.socket->send(client_msg);

      if (idx_BEGIN_TABLE != string::npos)
      {
        read_table(buffer.substr(idx_BEGIN_TABLE));
      }
    }
  }
}

void MonitoringService::read_table(const string &buffer)
{
  StringEqComparerIgnoreCase string_equals;
  const char delimiter = '\t';
  ParticipantTable &participants = node->participants;

  int prev_table_size = participants.map.size();
  std::vector<string> arr = splitString(buffer, delimiter);
  ptrdiff_t end = find(arr.begin(), arr.end(), "END TABLE") - arr.begin();
  std::span<string> table_span = std::span<string>(arr).subspan(2, end - 2);
  size_t i = 0;
  while (i < table_span.size())
  {
    participant_t part;
    sockaddr_in ipv4 = {};
    MachineEndpoint recieved_machine{};
    memset(&ipv4, 0, sizeof(ipv4));
    ipv4.sin_family = AF_INET;

    memcpy(recieved_machine.mac.mac_addr, table_span[i++].data(), MAC_ADDR_MAX); // add mac_addr (unsigned char*)
    memcpy(recieved_machine.mac.mac_str, table_span[i++].data(), MAC_STR_MAX);   // add mac_str (char*)
    ipv4.sin_port = htons(stoi(table_span[i++]));                                // port
    ipv4.sin_addr.s_addr = inet_addr(table_span[i++].c_str());                   // address
    recieved_machine.hostname = table_span[i++];                                 // hostname
    bool recieved_status = stoi(table_span[i++]) && true;                        // status
    time_t received_time_last = std::stol(table_span[i++]);                      // last_conection_timestamp
    int recieved_id = stoi(table_span[i++]);                                     // identification
    bool recieved_is_manager = stoi(table_span[i++]) && true;                    // add is_manager
    int manager_id = stoi(table_span[i++]);                                      // add manager_id

    recieved_machine.socket_address = *(sockaddr *)&ipv4;
    node->manager_id = manager_id;

    if (string_equals(recieved_machine.hostname, node->info.machine.hostname))
    {
      node->info.id = recieved_id;
      node->info.status = recieved_status;
      node->info.last_conection_timestamp = received_time_last;
      node->info.is_manager = recieved_is_manager;
      node->info.machine = recieved_machine;
      participants.map[recieved_machine.hostname] = node->info;
    }
    else
    {
      auto &map = participants.map;
      auto [it, inserted] = map.emplace(recieved_machine.hostname, participant_t{});
      if (inserted)
      {
        LOGF("Adding new machine %s, id %d", recieved_machine.to_string().c_str(), recieved_id);
        auto &[host, p] = *it;
        p.last_conection_timestamp = received_time_last;
        p.socket = std::make_unique<Socket>();
        p.id = recieved_id;
        p.is_manager = recieved_is_manager;
        p.status = recieved_status;
        p.machine = recieved_machine;
      }
      else
      {
        auto &p = map[recieved_machine.hostname];
        p.last_conection_timestamp = received_time_last;
      }
    }
  }
  int new_table_size = participants.map.size();
  if (new_table_size != prev_table_size)
  {
    LOGF("New table size %d", new_table_size);
  }

  for (auto &[host, participant] : participants.map)
  {
    if (participant.socket->file_descriptor == -1 && participant.id > node->info.id)
    {
      node->connect_peer(participant);
    }
  }
}

string MonitoringService::serialize_participant(const participant_t &participant)
{
  std::string stringified_table;
  std::string p_mac_addr(reinterpret_cast<const char *>(participant.machine.mac.mac_addr), sizeof(participant.machine.mac.mac_addr)); // unsigned char*
  std::string p_mac_str(reinterpret_cast<const char *>(participant.machine.mac.mac_str), sizeof(participant.machine.mac.mac_str));    // char*
  stringified_table += p_mac_addr + "\t";
  stringified_table += p_mac_str + "\t";
  stringified_table += std::to_string(((sockaddr_in *)&participant.machine.socket_address)->sin_port) + "\t"; // port
  stringified_table += inet_ntoa(((sockaddr_in *)&participant.machine.socket_address)->sin_addr);             // address
  stringified_table += "\t" + participant.machine.hostname + "\t";                                            // std::string
  stringified_table += std::to_string(participant.status) + "\t";                                             // bool
  stringified_table += std::to_string(participant.last_conection_timestamp) + "\t";                           // time_t
  stringified_table += std::to_string(participant.id) + "\t";                                                 // int
  stringified_table += std::to_string(participant.is_manager) + "\t";                                         // bool
  stringified_table += std::to_string(node->manager_id) + "\t";                                               // int
  return stringified_table;
}

string MonitoringService::serialize_table()
{
  ParticipantTable &participants = node->participants;
  std::string stringified_table = "BEGIN TABLE\t" + std::to_string(participants.clock) + "\t";
  for (auto &[host, participant] : participants.map)
  {
    stringified_table += serialize_participant(participant);
  }
  stringified_table += "END TABLE\t";
  return stringified_table;
}

void MonitoringService::send_msg_if(const string &msg, std::function<bool(participant_t &)> predicate)
{
  ParticipantTable &participants = node->participants;
  for (auto &[host, participant] : participants.map)
  {
    if (!predicate(participant) || participant.socket->file_descriptor < 0)
      continue;
    if (participant.socket->send(msg) < 0)
    {
      perrorcode("send");
    }
  }
}

void MonitoringService::update_peers_status(time_t timeout)
{
  to_remove.clear();
  ParticipantTable &participants = node->participants;
  time_t epoch_now = time(NULL);
  for (auto host : to_remove)
  {
    participants.remove(host);
    participants.dirty = true;
  }
  for (auto &[host, participant] : participants.map)
  {
    if (node->my_self(participant))
      continue;

    auto elapsed = participant.last_conection_timestamp - epoch_now;
    bool peer_awake = elapsed + timeout >= 0;
    participants.update_status(host, peer_awake);
  }
}

void MonitoringService::collect_file_descriptors()
{
  file_descriptors.clear();
  ParticipantTable &participants = node->participants;
  for (auto &[host, participant] : participants.map)
  {
    if (!node->my_self(participant) && participant.socket->file_descriptor > -1)
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

void MonitoringService::mark_as_deleted(const string &host)
{
  to_remove.push_back(host);
}

#endif // MONITORING_SERVICE_IMPLEMENTATION
#endif // MONITORING_SERVICE_H_
