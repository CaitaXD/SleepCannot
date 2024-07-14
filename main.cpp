#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
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
#include <pthread.h>
#include <string>

#define STR_IMPLEMENTATION
#include "str.h"
#undef STR_IMPLEMENTATION

#define SOCKET_IMPLEMENTATION
#include "Sockets/Sockets.h"
#undef SOCKET_IMPLEMENTATION

#define COMMANDS_IMPLEMENTATION
#include "commands.h"
#undef COMMANDS_IMPLEMENTATION

#define DISCOVERY_SERVICE_IMPLEMENTATION
#include "discovery_service.h"
#undef DISCOVERY_SERVICE_IMPLEMENTATION

bool key_hit() {
  struct timeval tv = {0, 0};
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);
  return select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) == 1;
}

int server() {
  printf("Server Side\n\n");
  help_msg(NULL);

  Socket s = socket_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  int broadcastEnable=1;
  int ret=setsockopt(s.fd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
  if(ret < 0)
  {
    perror("Fail to sned to endpoint");
    return -1;
  }
  
  EndPoint ep = ip_broadcast(PORT);
  socket_bind(&s, ep);

  ds_start_dicovering(s);

  Clients clients = Clients();
  while (1) {
    if (key_hit()) {
      s.error |= parse_command(clients, &s);
    }
    if (s.error) goto finally;

    msleep(500);
  }

  if (s.error) goto finally;

  finally:
    ds_stop_dicovering();
    s.error |= close(s.fd);
    return s.error;
}

int client() {
  Socket s = socket_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  int broadcastEnable=1;
  int ret=setsockopt(s.fd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
  if(ret < 0) {
    perror("Fail to sned to endpoint");
    return -1;
  }

  EndPoint ep = ip_broadcast(PORT);
  char bff[MAXLINE];
  Str buffer = STR(bff);
  ssize_t result = socket_send_endpoint(&s, client_msg, &ep, 0);
  if(result < 0) {
    perror("Fail to sned to endpoint");
    return -1;
  }

  while (1) {
    int read = socket_receive_endpoint(&s, buffer, &ep, 0);
    Str msg = str_take(buffer, read);

    if (str_cmp(msg, server_msg) == 0) {
      socket_send_endpoint(&s, client_msg, &ep, 0);
    } else if (str_starts_with(msg, commands[COMMAND_WAKE_ON_LAN].cmd)) {
      MacAddress mac = get_mac();
      Str mac_pckg = {
          .data = (char *)&mac,
          .len = sizeof(MacAddress),
      };

      int r = socket_send_endpoint(&s, mac_pckg, &ep, 0);
      if (r < 0) {
        perror("Error");
        exit(EXIT_FAILURE);
      } else {
        printf("Ive sensed a disturbance in the force\n");
      }
    }

    if (s.error) {
      printf("Error: %zu\n", s.error);
      perror("Error");
      exit(EXIT_FAILURE);
    }
  }

  close(s.fd);

  return 0;
}

int main(int argc, char **argv) {
  bool is_server = argc > 1 && !strcmp(argv[1], "server");

  ssize_t exit_code;
  if (is_server) {
    if ((exit_code = server()) != 0) {
      printf("Exit Code: %zi \n", exit_code);
    }
  } else {
    if ((exit_code = client()) != 0) {
      printf("Exit Code: %zi \n", exit_code);
    }
  }

  if (errno != 0) {
    perror("errno:");
  }
  return exit_code;
}
