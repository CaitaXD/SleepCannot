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
  void replicate_table();
  void update_peers_status(time_t timeout = 5);
  void collect_file_descriptors();
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
    Node &node = *m->node;
    ParticipantTable &participants = node.participants;
    while(m->running) {
      //participants.lock();
      {
        auto size = participants.map.size();
        if (size == 0) {
          //participants.unlock();
          rsleep(); // Let other threads get the GODDAMN MUTEX
          continue;
        }
        m->monitor_peers();
      }
      //participants.unlock();
      rsleep(); // Let other threads get the GODDAMN MUTEX
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

  if (node->is_manager())
  {
    replicate_table();
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

    auto &[perr_name, peer_refrence] = optional_peer.value();
    auto &peer = peer_refrence.get();
    string buffer(1024, '\0');
    int read = sock.recv(&buffer);

    if (read <= 0)
    {
      if (read < 0)
        perrorcode("recv");
      continue;
    }

    if (string_equals(buffer, "exit"))
    {
      LOGF("Exiting from %s", perr_name.c_str());
      sock.close();
      to_remove.push_back(perr_name);
      participants.dirty = true;
      continue;
    }

    if (node->is_election_message(buffer))
      node->handle_election_response(buffer);

    if (node->is_manager())
    {
      peer.last_conection_timestamp = time(NULL);
      node->info.last_conection_timestamp = time(NULL);
    }
    else
    {
      FUZZ_DELAY;
      read = peer.socket->send(client_msg);

      size_t table_start = buffer.rfind("BEGIN TABLE", 0);
      if (table_start != string::npos)
      {
        read_table(buffer.substr(table_start));
      }
    }
  }

  if (!node->is_manager())
  {
    auto optional_manager = participants.find_manager();
    if (!optional_manager.has_value())
      return;

    auto &[perr_name, manager] = optional_manager.value();
    if ((time(NULL) - manager.get().last_conection_timestamp) * 1000 >= TIMEOUT_ELECTION)
    {
      node->run_election();
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
    MachineEndpoint recieved_machine{};
    memcpy(recieved_machine.mac.mac_addr, table_span[i++].data(), MAC_ADDR_MAX); // add mac_addr (unsigned char*)
    memcpy(recieved_machine.mac.mac_str, table_span[i++].data(), MAC_STR_MAX);   // add mac_str (char*)
    sockaddr_in ipv4 = {};
    memset(&ipv4, 0, sizeof(ipv4));
    ipv4.sin_family = AF_INET;
    ipv4.sin_port = htons(stoi(table_span[i++]));              // add id_address (idk)
    ipv4.sin_addr.s_addr = inet_addr(table_span[i++].c_str()); // add id_address (idk)
    recieved_machine.socket_address = *(sockaddr *)&ipv4;
    recieved_machine.hostname = table_span[i++];     // add hostname (std::string)
    bool status = stoi(table_span[i++]) && true;     // add status (bool)
    time_t time_last = stoi(table_span[i++]);        // add last_conection_timestamp (time_t)
    int identification = stoi(table_span[i++]);      // add identification (int)
    bool is_manager = stoi(table_span[i++]) && true; // add is_manager (bool)

    participant_t recieved_part = participant_t{
        .machine = recieved_machine,
        .status = status,
        .socket = std::make_shared<Socket>(),
        .last_conection_timestamp = time_last,
        .id = identification,
        .is_manager = is_manager};

    if (string_equals(recieved_machine.hostname, node->info.machine.hostname))
    {
      node->info.id = identification;
      node->info.machine = recieved_machine;
      node->info.status = status;
      node->info.last_conection_timestamp = time_last;
      node->info.is_manager = is_manager;
      recieved_part.socket = node->info.socket; // Server socket
      participants.map[recieved_machine.hostname] = recieved_part;
    }
    else
    {
      participants.get_or_add(recieved_machine.hostname, recieved_part);
      auto &peersock = participants.get_or_add(recieved_machine.hostname, recieved_part).socket;
      if (peersock->file_descriptor == -1)
      {
        auto sock = node->connect_peer(recieved_machine); // Client socket
        recieved_part.socket = std::make_shared<Socket>(std::move(sock));
      }
      else
      {
        recieved_part.socket = peersock;
      }
    }
  }
  int new_table_size = participants.map.size();
  if (new_table_size != prev_table_size)
  {
    LOGF("New table size %d", new_table_size);
  }
}

void MonitoringService::replicate_table()
{
  ParticipantTable &participants = node->participants;
  participants.lock();
  std::string stringified_table = "BEGIN TABLE\t" + std::to_string(participants.clock) + "\t";

  for (auto &[host, participant] : participants.map)
  {
    std::string p_mac_addr(reinterpret_cast<char *>(participant.machine.mac.mac_addr), sizeof(participant.machine.mac.mac_addr)); // unsigned char*
    stringified_table += p_mac_addr + "\t";
    std::string p_mac_str(reinterpret_cast<char *>(participant.machine.mac.mac_str), sizeof(participant.machine.mac.mac_str)); // char*
    stringified_table += p_mac_str + "\t";
    stringified_table += std::to_string(((sockaddr_in *)&participant.machine.socket_address)->sin_port) + "\t"; // port
    stringified_table += inet_ntoa(((sockaddr_in *)&participant.machine.socket_address)->sin_addr);             // address
    stringified_table += "\t" + participant.machine.hostname + "\t";                                            // std::string
    stringified_table += std::to_string(participant.status) + "\t";                                             // bool
    stringified_table += std::to_string(participant.last_conection_timestamp) + "\t";                           // time_t
    stringified_table += std::to_string(participant.id) + "\t";                                                 // int
    stringified_table += std::to_string(participant.is_manager) + "\t";                                         // bool
  }
  stringified_table += "END TABLE\t";

  for (auto &[host, participant] : participants.map)
  {
    FUZZ_DELAY;
    if (participant.socket->send(stringified_table) < 0)
      perrorcode("send");
  }
  participants.unlock();
}

void MonitoringService::update_peers_status(time_t timeout)
{
  ParticipantTable &participants = node->participants;
  time_t epoch_now = time(NULL);
  participants.lock();
  if (node->is_manager())
  {
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
  participants.unlock();
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

void MonitoringService::mark_as_deleted(const string &host)
{
  to_remove.push_back(host);
}

#endif // MONITORING_SERVICE_IMPLEMENTATION
#endif // MONITORING_SERVICE_H_
