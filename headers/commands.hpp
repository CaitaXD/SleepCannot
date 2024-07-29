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

int command_exec(ParticipantTable &participants)
{
  int exit_code = 0;
  char buffer[MAXLINE];
  fgets(buffer, MAXLINE, stdin);
  ssize_t read = strlen(buffer);
  string cmd = string(buffer).substr(0, read);
  trim(cmd);
  ascii_toupper(cmd);
  enum CommandType cmd_type = get_command_type(cmd);
  switch (cmd_type)
  {
  case COMMAND_WAKE_ON_LAN:
  {
    string cmd_args = string(cmd).substr(commands[cmd_type].cmd.size());
    trim(cmd_args);
    auto host_name = string(cmd_args).substr(0, cmd_args.find(" "));
    auto it = participants.map.find(host_name);
    if (it == participants.map.end())
    {
        std::cerr << "[ERROR] Invalid Hostname" << std::endl;
        break;
    }
    const auto &[host, participant] = *it;
    string magic_packet = "wakeonlan " + std::string(participant.machine.mac.mac_str);
    if (system(magic_packet.c_str()) < 0)
    {
      perrorcode("wakeonlan");
    }
    break;
  }
  default:
    std::cerr << "[ERROR] Invalid command " << cmd << std::endl;
    goto finally;
  }
finally:
  return exit_code;
}

#endif // COMMANDS_IMPLEMENTATION
