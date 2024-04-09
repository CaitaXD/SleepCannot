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

#define STR_IMPLEMENTATION
#include "str.h"
#undef STR_IMPLEMENTATION

#define SOCKET_IMPLEMENTATION
#include "Sockets/Sockets.h"
#undef SOCKET_IMPLEMENTATION

#define DYNAMIC_ARRAY_IMPLEMENTATION
#include "DataStructures/DynamicArray.h"
#undef DYNAMIC_ARRAY_IMPLEMENTATION

#define COMMANDS_IMPLEMENTATION
#include "commands.h"
#undef COMMANDS_IMPLEMENTATION

#define OBJECT_IMPLEMENTATION
#include "object.h"
#undef OBJECT_IMPLEMENTATION

Str client_msg = STR("General, Kenoby, you are a bold one");
Str server_msg = STR("Hello there!");

bool key_hit() {
  struct timeval tv = {0, 0};
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);
  return select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) == 1;
}

int server() {
  Clients clients = Clients();

  printf("Server Side\n\n");
  help_msg(NULL);

  Socket s = socket_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  EndPoint ep = ip_endpoint(INADDR_ANY, PORT);
  socket_bind(&s, ep);

  if (s.error) {
    goto finally;
  }

  Str buffer = STR((char[MAXLINE]){});
  int read;
  do {
    s.error = 0;
    errno = 0;
    read = socket_receive_endpoint(&s, buffer, &ep, MSG_DONTWAIT);
    if (key_hit()) {
      s.error |= parse_command(buffer, clients, &s);
    }
  } while (errno == EAGAIN);

  while (1) {
    if (str_cmp(str_take(buffer, read), client_msg) == 0) {
      if (!dynamic_array_contains(clients, ep, epcmp_inaddr)) {

        struct sockaddr_in *addr = (struct sockaddr_in *)&ep.addr;
        const char *ip = inet_ntoa(addr->sin_addr);
        int port = ntohs(addr->sin_port);

        printf("%s %s:%d\n", "New client, IP:", ip, port);
        dynamic_array_push(clients, ep);
      }
    }
    if (key_hit()) {
      s.error |= parse_command(buffer, clients, &s);
    }
    if (s.error) {
      goto finally;
    }
  }
finally:
  s.error |= close(s.fd);
  return s.error;
}

int client() {
  Socket s = socket_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  EndPoint ep = ip_endpoint(INADDR_ANY, PORT);

  Str buffer = STR((char[MAXLINE]){});
  socket_send_endpoint(&s, client_msg, &ep, 0);

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
