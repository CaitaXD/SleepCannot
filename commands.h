#ifndef COMMANDS_H_
#define COMMANDS_H_

#include "../object.h"
#include "DataStructures/DynamicArray.h"
#include "Sockets/Sockets.h"
#include "str.h"

#define MAC_ADDR_MAX 6
#define MAC_STR_MAX 64
#define MAC_ADDRES_FILE "/sys/class/net/eth0/address"
#define PORT 25565
#define MAXLINE 1024

typedef struct {
  unsigned char mac_addr[MAC_ADDR_MAX];
  char mac_str[MAC_STR_MAX];
} MacAddress;

typedef struct ClientsDinamicArray {
  DynamicArray(EndPoint);
} Clients;

#define Clients() ((Clients){})

typedef void *(*Callback)(void *);
typedef struct Command {
  Str cmd;
  const char *description;
  const char *fmt;
  const Callback callback;
} Command;

#define STRINGYFY(x) #x
enum CommandType {
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

struct list_clients_args {
  Clients clients;
};

struct ping_client_args {
  Clients clients;
  int id;
};

struct wake_on_lan_args {
  Socket *s;
  Clients clients;
  int id;
};

MacAddress get_macaddr();
void *wake_on_lan(struct wake_on_lan_args *args);
void *list_clients(struct list_clients_args *clients);
void *ping_client(struct ping_client_args *arg);
void *clear_screen(void *args);
void *exit_program(void *args);
void *help_msg(void *args);

#endif // COMMANDS_H_

#ifdef COMMANDS_IMPLEMENTATION

Command commands[COMMAND_COUNT] = {
    // clang-format off
    [COMMAND_WAKE_ON_LAN] = {
      .cmd = STR("Execute order 66"),
      .fmt = "%d",
      .description = "Sends a magic packet to the client to wake it up",
      .callback = (Callback)wake_on_lan
    },
    [COMMAND_EXIT] = {
      .cmd = STR("The negotiations were short"),
      .description = "Exits the program",
      .callback = (Callback)exit_program
    },
    [COMMAND_LIST] = {
      .cmd = STR("list"),
      .description = "Lists all subscribed clients",
      .callback = (Callback)list_clients
    },
    [COMMAND_CLEAR] = {
      .cmd = STR("clear"),
      .description = "Clears the screen",
      .callback = (Callback)clear_screen
    },
    [COMMAND_PING] = {
      .cmd = STR("ping"), 
      .description = "Pings a client",
      .fmt = "%d", 
      .callback = (Callback)ping_client
    },
    [COMMAND_HELP] = {
      .cmd = STR("help"),
      .description = "Prints this message",
      .callback = (Callback)help_msg
    }
    // clang-format on
};

enum CommandType get_command_type(Str command) {
  for (size_t i = 0; i < COMMAND_COUNT; i++) {
    if (str_starts_with(command, commands[i].cmd)) {
      return (enum CommandType)i;
    }
  }
  return COMMAND_ERROR;
}

int command_exec(enum CommandType cmd, void *command_args) {
  return (intptr_t)commands[cmd].callback(command_args);
}

void *list_clients(struct list_clients_args *args) {
  Clients clients = args->clients;
  if (clients.count == 0) {
    printf("[INFO] 0 clients connected\n");
    return NULL;
  }
  dynamic_array_for(i, clients) {
    EndPoint cep = *dynamic_array_at(clients, i);

    struct sockaddr_in *addr = (struct sockaddr_in *)&cep.addr;
    const char *ip = inet_ntoa(addr->sin_addr);
    int port = ntohs(addr->sin_port);

    printf("[ID: %zu] Client: %s:%d\n", i, ip, port);
  }

  return NULL;
}

void *ping_client(struct ping_client_args *arg) {
  int id = arg->id;
  Clients clients = arg->clients;

  if ((size_t)id < clients.count) {
    EndPoint cep = *dynamic_array_at(clients, id);
    char ping_cmd_buf[256];
    const char *iadrr = inet_ntoa(((struct sockaddr_in *)&cep.addr)->sin_addr);
    int n = snprintf(UNPACK_ARRAY(ping_cmd_buf), "ping -c 1 %s", iadrr);
    if (n > 0) {
      ping_cmd_buf[n] = '\0';
      system(ping_cmd_buf);
    } else {
      perror("snprintf error could not read socket address string");
      return (void *)-1;
    }
  } else {
    fprintf(stderr, "[ERROR] Invalid client id %d\n", id);
  }

  return NULL;
}

void *wake_on_lan(struct wake_on_lan_args *args) {
  int id = args->id;
  Clients clients = args->clients;
  Socket *s = args->s;

  if ((size_t)id < clients.count) {
    EndPoint cep = *dynamic_array_at(clients, id);
    socket_send_endpoint(s, commands[COMMAND_WAKE_ON_LAN].cmd, &cep, 0);
    Str buffer = STR((char[MAXLINE]){});
    int read = socket_receive_endpoint(s, buffer, &cep, 0);
    if (read <= 0) {
      return (void *)-1;
    }

    MacAddress mac = *(MacAddress *)buffer.data;
    Str mac_str = STR(mac.mac_str);
    printf("MAC: " str_fmt "\n", str_args(mac_str));
    printf("[WARNING] WoL Not implemented!\n");
    return NULL;
  } else {
    fprintf(stderr, "[ERROR] Invalid client id %d\n", id);
  }

  return NULL;
}

void *clear_screen(void *args) {
  (void)args;
  printf("\033[H\033[J\n");
  return NULL;
}

void *exit_program(void *args) {
  exit((intptr_t)args);
  return NULL;
}

void *help_msg(void *args) {
  (void)args;
  for (size_t i = 0; i < COMMAND_COUNT; i++) {
    printf("[COMMAND]: " str_fmt "\n", str_args(commands[i].cmd));
    if (commands[i].description) {
      printf("[DESCRIPTION]: %s\n", commands[i].description);
    }
    if (commands[i].fmt) {
      printf("[FORMAT]: %s\n", commands[i].fmt);
    }
    printf("\n");
  }
  return NULL;
}

int parse_command(Str buffer, Clients clients, Socket *s) {
  int exit_code = 0;
  buffer.data[0] = '\0';
  fgets(str_unpack(buffer), stdin);
  ssize_t read = strlen(buffer.data);
  Str cmd = str_trim(str_take(buffer, read));
  enum CommandType cmd_type = get_command_type(cmd);
  void *args = NULL;
  switch (cmd_type) {
  case COMMAND_HELP:
  case COMMAND_EXIT:
  case COMMAND_CLEAR: {
    break;
  }
  case COMMAND_LIST: {
    args = &(struct list_clients_args){.clients = clients};
    break;
  }
  case COMMAND_PING: {
    int id;
    Str cmd_args = str_trim(str_skip(cmd, commands[cmd_type].cmd.len));
    if (sscanf(cmd_args.data, commands[cmd_type].fmt, &id) == 1) {
      args = &(struct ping_client_args){.clients = clients, .id = id};
    } else {
      exit_code = -1;
      goto finally;
    }
    break;
  }
  case COMMAND_WAKE_ON_LAN: {
    int id = 0;
    Str cmd_args = str_trim(str_skip(cmd, commands[cmd_type].cmd.len));
    if (sscanf(cmd_args.data, commands[cmd_type].fmt, &id) == 1) {
      args = &(struct wake_on_lan_args){.s = s, .clients = clients, .id = id};
    } else {
      exit_code = -1;
      goto finally;
    }
    break;
  }
  default:
    fprintf(stderr, "[ERROR] Invalid command " str_fmt "\n", str_args(cmd));
    goto finally;
  }
  exit_code |= command_exec(cmd_type, args);

finally:
  return exit_code;
}

MacAddress get_mac() {
  MacAddress mac = {};

  FILE *f = fopen(MAC_ADDRES_FILE, "r");
  if (f == NULL) {
    perror("Error opening file");
    exit(EXIT_FAILURE);
  }

  char mac_str[MAC_STR_MAX];
  fscanf(f, "%s", mac_str);
  fclose(f);

  sscanf(mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac.mac_addr[0],
         &mac.mac_addr[1], &mac.mac_addr[2], &mac.mac_addr[3], &mac.mac_addr[4],
         &mac.mac_addr[5]);

  printf("MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n", mac.mac_addr[0],
         mac.mac_addr[1], mac.mac_addr[2], mac.mac_addr[3], mac.mac_addr[4],
         mac.mac_addr[5]);

  mac.mac_str[0] = '\0';
  for (int i = 0; i < MAC_ADDR_MAX; i++) {
    snprintf(mac.mac_str + strlen(mac.mac_str),
             MAC_STR_MAX - strlen(mac.mac_str), "%02x%s", mac.mac_addr[i],
             i < MAC_ADDR_MAX - 1 ? ":" : "");
  }
  return mac;
}

#endif // COMMANDS_IMPLEMENTATION