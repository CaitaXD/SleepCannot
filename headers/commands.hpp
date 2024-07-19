#ifndef COMMANDS_H_
#define COMMANDS_H_

#include <set>
#include <vector>
#include "Net/Socket.hpp"
#include <condition_variable>
#include <iostream>
#include <unordered_map>
#include <string.h>
#include <string>
#include <mutex>
#include "macros.h"
#include "management.hpp"

typedef void *(*Callback)(void *);
typedef struct Command
{
  string_view cmd;
  string_view description;
  string_view fmt;
  const Callback callback;
} Command;

enum CommandType
{
  COMMAND_ERROR = -1,
  COMMAND_WAKE_ON_LAN,
  COMMAND_COUNT,
};

extern Command commands[COMMAND_COUNT];


void *help_msg_server();
void *help_msg_client();
int command_exec(ParticipantTable &participants);

#endif // COMMANDS_H_
#ifdef COMMANDS_IMPLEMENTATION

Command commands[COMMAND_COUNT] = {
    // clang-format off
    [COMMAND_WAKE_ON_LAN] = Command{
      cmd : "WAKEUP",
      description : "Sends a magic packet to the client to wake it up",
      fmt : "%d",
      callback : NULL
    }
    // clang-format on
};

enum CommandType get_command_type(string_view command)
{
  for (size_t i = 0; i < COMMAND_COUNT; i++)
  {
    if (command.rfind(commands[i].cmd) == 0)
    {
      return (enum CommandType)i;
    }
  }
  return COMMAND_ERROR;
}

void *clear_screen(void *args)
{
  (void)args;
  printf("\033[H\033[J\n");
  return NULL;
}

void *exit_program(void *args)
{
  exit((intptr_t)args);
  return NULL;
}

void *help_msg_server()
{
  printf("%s", "[COMMAND]\tWAKEUP <hostname>\n");
  printf("%s", "[DESCRIPTION]\tSends a WoL packet to <hostname> connected to the service.\n\n");

  return NULL;
}

void *help_msg_client()
{
  printf("%s", "[COMMAND]\tEXIT\n");
  printf("%s", "[DESCRIPTION]\tExists the program.\n\n");

  return NULL;
}

inline void ltrim(string &s)
{
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch)
                                  { return !std::isspace(ch); }));
}

inline void rtrim(string &s)
{
  s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch)
                       { return !std::isspace(ch); })
              .base(),
          s.end());
}

inline void trim(string &s)
{
  rtrim(s);
  ltrim(s);
}

int command_exec(ParticipantTable &participants)
{
  int exit_code = 0;
  char buffer[MAXLINE];
  fgets(buffer, MAXLINE, stdin);
  ssize_t read = strlen(buffer);
  string cmd = string(buffer).substr(0, read);
  trim(cmd);
  enum CommandType cmd_type = get_command_type(cmd);
  participants.lock();
  switch (cmd_type)
  {
  case COMMAND_WAKE_ON_LAN:
  {
    string cmd_args = string(cmd).substr(commands[cmd_type].cmd.size());
    trim(cmd_args);
    auto host_name = string(cmd_args).substr(0, cmd_args.find(" "));
    auto participant = participants.get(host_name);
    string magic_packet = "wakeup " + host_name;
    IpEndpoint broadcast = IpEndpoint::broadcast(INITIAL_PORT + 2);
    Socket s{};
    s = s.open(AddressFamily::InterNetwork, SocketType::Datagram, SocketProtocol::UDP);
    if (s.bind(IpEndpoint{INADDR_ANY, INITIAL_PORT + 2}) < 0)
    {
      perror("bind");
    }
    int r = s.send(magic_packet, broadcast, 0);
    if (r < 0)
    {
      perror("wake_on_lan");
    }
    else
    {
      std::cout << "[INFO] Sent magic packet to " << host_name << std::endl;
    }
    break;
  }
  default:
    std::cerr << "[ERROR] Invalid command " << cmd << std::endl;
    goto finally;
  }
finally:
  participants.unlock();
  return exit_code;
}

#endif // COMMANDS_IMPLEMENTATION