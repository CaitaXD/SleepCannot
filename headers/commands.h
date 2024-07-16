#ifndef COMMANDS_H_
#define COMMANDS_H_

#include "Sockets.h"
#include "DataStructures/str.h"
#include <set>
#include <vector>
#include "Socket.hpp"
#include <mutex>
#include <condition_variable>

#define MAC_ADDR_MAX 6
#define MAC_STR_MAX 64
#define MAC_ADDRES_FILE "/sys/class/net/eth0/address"
#define MAXLINE 1024
#define INITIAL_PORT 35512

Str client_msg = STR("General, Kenoby, you are a bold one");
Str server_msg = STR("Hello there!");

typedef struct
{
  unsigned char mac_addr[MAC_ADDR_MAX];
  char mac_str[MAC_STR_MAX];
} MacAddress;

// Represents a participant using the service
typedef struct participant_t
{
  std::string hostname;
  MacAddress mac;
  std::string ip;
  bool status; // true means awake, false means asleep
} participant_t;

// Represents the table of users using the service
typedef std::unordered_map<std::string, participant_t> ParticipantTable;

struct MachineEndpoint : EndPoint
{
  MacAddress mac;
  std::string hostname;

  MachineEndpoint with_port(int port) const
  {
    MachineEndpoint ep = *this;
    ep.addrlen = sizeof(ep.addr);
    struct sockaddr_in *addr = (struct sockaddr_in *)&ep.addr;
    addr->sin_port = htons(port);
    return ep;
  }
};

typedef std::vector<MachineEndpoint> Clients;

typedef void *(*Callback)(void *);
typedef struct Command
{
  Str cmd;
  const char *description;
  const char *fmt;
  const Callback callback;
} Command;

#define STRINGYFY(x) #x
enum CommandType
{
  COMMAND_ERROR = -1,
  COMMAND_WAKE_ON_LAN,
  COMMAND_EXIT,
  COMMAND_LIST,
  COMMAND_CLEAR,
  COMMAND_PING,
  COMMAND_HELP,
  COMMAND_COUNT,
};

extern Command commands[COMMAND_COUNT];

struct list_clients_args
{
  Clients clients;
};

struct ping_client_args
{
  Clients clients;
  int id;
};

struct wake_on_lan_args
{
  Socket *s;
  participant_t participant;
};

MacAddress get_mac();
void *cmd_wake_on_lan(struct wake_on_lan_args *args);
void *list_clients(struct list_clients_args *clients);
void *ping_client(struct ping_client_args *arg);
void *clear_screen(void *args);
void *exit_program(void *args);
void *help_msg(void *args);

#endif // COMMANDS_H_

#ifdef COMMANDS_IMPLEMENTATION

// Theese are not used in the program, only in testing
Command commands[COMMAND_COUNT] = {
    // clang-format off
    [COMMAND_WAKE_ON_LAN] = {
      cmd : STR("WAKEUP"),
      description : "Sends a magic packet to the client to wake it up",
      fmt : "%d",
      callback : NULL
    },
    [COMMAND_EXIT] = {
      cmd : STR("The negotiations were short"),
      description : "Exits the program",
      fmt : NULL,
      callback : (Callback)exit_program
    },
    [COMMAND_LIST] = {
      cmd : STR("list"),
      description : "Lists all subscribed clients",
      fmt : NULL,
      callback : (Callback)list_clients
    },
    [COMMAND_CLEAR] = {
      cmd : STR("clear"),
      description : "Clears the screen",
      fmt : NULL,
      callback : (Callback)clear_screen
    },
    [COMMAND_PING] = {
      cmd : STR("ping"), 
      description : "Pings a client",
      fmt : "%d", 
      callback : (Callback)ping_client
    },
    [COMMAND_HELP] = {
      cmd : STR("help"),
      description : "Prints this message",
      fmt : NULL,
      callback : (Callback)help_msg
    }
    // clang-format on
};

enum CommandType get_command_type(Str command)
{
  for (size_t i = 0; i < COMMAND_COUNT; i++)
  {
    if (str_starts_with(command, commands[i].cmd))
    {
      return (enum CommandType)i;
    }
  }
  return COMMAND_ERROR;
}

int command_exec(enum CommandType cmd, void *command_args)
{
  if (commands[cmd].callback != nullptr)
    return (intptr_t)commands[cmd].callback(command_args);
  return 0;
}

void *list_clients(struct list_clients_args *args)
{
  Clients clients = args->clients;
  if (clients.size() == 0)
  {
    printf("[INFO] 0 clients connected\n");
    return NULL;
  }
  for (size_t i = 0; i < clients.size(); i++)
  {
    EndPoint cep = clients[i];
    struct sockaddr_in *addr = (struct sockaddr_in *)&cep.addr;
    const char *ip = inet_ntoa(addr->sin_addr);
    int port = ntohs(addr->sin_port);

    printf("[ID: %zu] Client: %s:%d\n", i, ip, port);
  }

  return NULL;
}

void *ping_client(struct ping_client_args *arg)
{
  int id = arg->id;
  Clients clients = arg->clients;

  if ((size_t)id < clients.size())
  {
    EndPoint cep = clients[id];
    char ping_cmd_buf[256];
    const char *iadrr = inet_ntoa(((struct sockaddr_in *)&cep.addr)->sin_addr);
    int n = snprintf(UNPACK_ARRAY(ping_cmd_buf), "ping -c 1 %s", iadrr);
    if (n > 0)
    {
      ping_cmd_buf[n] = '\0';
      system(ping_cmd_buf);
    }
    else
    {
      perror("snprintf error could not read UdpSocket address string");
      return (void *)-1;
    }
  }
  else
  {
    fprintf(stderr, "[ERROR] Invalid client id %d\n", id);
  }

  return NULL;
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

void *help_msg(void *args)
{
  (void)args;
  printf("\nCommands used for testing and debugging:\n");
  for (size_t i = 0; i < COMMAND_COUNT; i++)
  {
    printf("[COMMAND]: " str_fmt "\n", str_args(commands[i].cmd));
    if (commands[i].description)
    {
      printf("[DESCRIPTION]: %s\n", commands[i].description);
    }
    if (commands[i].fmt)
    {
      printf("[FORMAT]: %s\n", commands[i].fmt);
    }
    printf("\n");
  }
  return NULL;
}

void *help_msg_server()
{
  printf("[COMMAND]\tWAKEUP <hostname>\n");
  printf("[DESCRIPTION]\tSends a WoL packet to <hostname> connected to the service.\n\n");

  return NULL;
}

void *help_msg_client()
{
  printf("[COMMAND]\tEXIT\n");
  printf("[DESCRIPTION]\tExists the program.\n\n");

  return NULL;
}

int parse_command(ParticipantTable &participants, std::mutex &mutex)
{
  int exit_code = 0;
  char bff[MAXLINE];
  Str buffer = STR(bff);
  fgets(bff, MAXLINE, stdin);
  ssize_t read = strlen(bff);
  Str cmd = str_trim(str_take(buffer, read));
  enum CommandType cmd_type = get_command_type(cmd);
  //void *args = NULL;
  std::lock_guard<std::mutex> lock(mutex);
  switch (cmd_type)
  {
  case COMMAND_HELP:
  case COMMAND_EXIT:
  case COMMAND_CLEAR:
  case COMMAND_LIST:
  case COMMAND_PING:
  case COMMAND_WAKE_ON_LAN:
  {
    Str cmd_args = str_trim(str_skip(cmd, commands[cmd_type].cmd.len));
    auto host_name = string_from_str(str_take(cmd_args, cmd_args.len));
    auto participant = participants.at(host_name);
    std::string magic_packet = "wakeup " + host_name;
    EndPoint broadcast = ip_broadcast(INITIAL_PORT + 2);
    UdpSocket s = UdpSocket_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(UdpSocket_bind(&s, ip_endpoint(INADDR_ANY, INITIAL_PORT + 2)) < 0){
      perror("bind");
    }
    int r = UdpSocket_send_endpoint(&s, str_from_string(magic_packet), &broadcast, 0);
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
    fprintf(stderr, "[ERROR] Invalid command " str_fmt "\n", str_args(cmd));
    goto finally;
  }
finally:
  std::lock_guard<std::mutex> unlock(mutex);
  return exit_code;
}

MacAddress get_mac()
{
  MacAddress mac = {};

  FILE *f = fopen(MAC_ADDRES_FILE, "r");
  if (f == NULL)
  {
    perror("Error opening file");
    exit(EXIT_FAILURE);
  }

  char mac_str[MAC_STR_MAX];
  fscanf(f, "%s", mac_str);
  fclose(f);

  sscanf(mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac.mac_addr[0],
         &mac.mac_addr[1], &mac.mac_addr[2], &mac.mac_addr[3], &mac.mac_addr[4],
         &mac.mac_addr[5]);

  mac.mac_str[0] = '\0';
  for (int i = 0; i < MAC_ADDR_MAX; i++)
  {
    snprintf(mac.mac_str + strlen(mac.mac_str),
             MAC_STR_MAX - strlen(mac.mac_str), "%02x%s", mac.mac_addr[i],
             i < MAC_ADDR_MAX - 1 ? ":" : "");
  }
  return mac;
}

#endif // COMMANDS_IMPLEMENTATION