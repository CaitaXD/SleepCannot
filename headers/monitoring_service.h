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

#ifdef MONITORING_SERVICE_IMPLEMENTATION

std::vector<std::string> splitString(std::string &input, char delimiter) {
  std::istringstream stream(input);

  std::string token;

  std::vector<std::string> arr;

  while(std::getline(stream, token, delimiter)){
    arr.push_back(token);
  }

  return arr;
}

void MonitoringService::start_service()
{
  if (running)
    return;
  running = true;

  pthread_create(&thread, NULL, [](void *data) -> void *
                 {
    StringEqComparerIgnoreCase string_equals;
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
      if(participants->send_table){
        std::string stringified_table = "table\t"+std::to_string(participants->clock)+"\t";

        for (auto &[host, participant] : participants->map){
            std::string p_mac_addr(reinterpret_cast<char*>(participant.machine.mac.mac_addr), sizeof(participant.machine.mac.mac_addr)); // unsigned char*
            stringified_table += p_mac_addr+"\t";
            std::string p_mac_str(reinterpret_cast<char*>(participant.machine.mac.mac_str), sizeof(participant.machine.mac.mac_str)); // char*
            stringified_table += p_mac_str+"\t";
            stringified_table += inet_ntoa(((sockaddr_in *)&participant.machine.socket_address)->sin_addr); // ip (idk the type)
            stringified_table += "\t"+participant.machine.hostname+"\t"; // std::string
            stringified_table += std::to_string(participant.status)+"\t";  // bool
            stringified_table += std::to_string(participant.last_conection_timestamp)+"\t"; // time_t
            stringified_table += std::to_string(participant.id) // int
        }

        for (auto &[host, participant] : participants->map){
            participant.socket->send(stringified_table);
        }

        std::cout << stringified_table << std::endl;
        
        participants->send_table = false;
      }
    }
    else
    {
      char delimiter = '\t';
      std::vector<std::string> arr = splitString(read, delimiter);
      std::cout << arr.at(0) << std::endl;
      std::cout << cmd+"test" << std::endl;
      if (!arr.at(0).compare("table") || !arr.at(0).compare("probe from servertable")){
        std::cout << "clock: " << arr.at(1) << std::endl;
        long unsigned int i = 2;
        while(i < arr.size()) {
          participant_t part;
          MachineEndpoint machine{};
          memcpy(machine.mac.mac_addr, arr.at(i++).data(), MAC_ADDR_MAX); // add mac_addr (unsigned char*)
          memcpy(machine.mac.mac_str, arr.at(i++).data(), MAC_STR_MAX); // add mac_str (char*)
          machine.socket_address = inet_addr(arr.at(i++)); // add id_address (idk)
          machine.hostname = arr.at(i++); // add hostname (std::string)
          bool status = stoi(arr.at(i++)) && true; // add status (bool)
          time_t time_last = stoi(arr.at(i++)); // add last_conection_timestamp (time_t)
          int identification = stoi(arr.at(i++)) // add identification (int)

          participant_t participant = participant_t{
                    .machine = machine,
                    .status = status,
                    .socket = std::make_shared<Socket>(),
                    .last_conection_timestamp = time_last,
                    .id = identification,
                    .is_manager = false};
        }
        participants->map.at(machine.hostname) = participant;
      } 
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
