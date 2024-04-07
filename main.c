#include <arpa/inet.h>
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

#define DYNAMIC_ARRAY_IMPLEMENTATION
#include "DataStructures/DynamicArray.h"

#define PORT 25565
#define MAXLINE 1024
#define CONCOLE_CLEAR() printf("\033[H\033[J\n")

typedef struct ClientsDynamicArray {
  DynamicArray(EndPoint);
} Clients;

Str client_msg = STR("General, Kenoby, you are a bold one");
Str server_msg = STR("Hello there!");

Str cmd_list = STR("list");
Str cmd_exit = STR("the negotiations were short");
Str cmd_clear = STR("clear");

bool key_hit() {
  struct timeval tv = {0, 0};
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);
  return select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) == 1;
}

void list_clients(Clients clients) {
  dynamic_array_foreach(EndPoint, cep, clients) {
    struct sockaddr_in *addr = (struct sockaddr_in *)&cep.addr;
    printf("Client: %s:%d\n", inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
    sleep(1);
  }
}

int server() {
  int exit_code = 0;

  Clients clients = {};
  Socket s = socket_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  EndPoint ep = ip_endpoint(INADDR_ANY, PORT);
  socket_bind(&s, ep);

  if (s.error) {
    printf("Error: %zu\n", s.error);
    perror("Error");
    exit(EXIT_FAILURE);
  }

  Str buffer = STR((char[MAXLINE]){});
  while (1) {
    Str hello = STR("Hello, There!");
    int read = socket_receive_endpoint(&s, buffer, &ep, 0 );
    if (read < 0) {
      s.error = 0;
      continue;
    }

    socket_send_endpoint(&s, hello, &ep, 0);

    if (str_cmp(str_take(buffer, read), client_msg) == 0) {
      if (!dynamic_array_contains(clients, ep, epcmp_inaddr)) {
        printf("%s\n", "New client!");
        dynamic_array_push(clients, ep);
      }
    }

    if (s.error) {
      printf("Error: %zu\n", s.error);
      perror("Error");
      exit(EXIT_FAILURE);
    }

    if (key_hit()) {
      char ln[256];
      fgets(ln, sizeof(ln), stdin);
      if (strncmp(ln, str_unpack(cmd_list)) == 0) {
        list_clients(clients);
      } else if (strncmp(ln, str_unpack(cmd_clear)) == 0) {
        CONCOLE_CLEAR();
      } else if (strncmp(ln, str_unpack(cmd_exit)) == 0) {
        goto finally;
      }
    }
  }

finally:
  exit_code |= close(s.fd);
  if ((s.error != 0) | (exit_code != 0)) {
    printf("Error: %zu\n", s.error);
    perror("Error");
    exit(EXIT_FAILURE);
  }
  return exit_code;
}

int client() {
  Socket s = socket_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  EndPoint ep = ip_endpoint(INADDR_ANY, PORT);

  Str buffer = STR((char[MAXLINE]){});
  while (1) {

    socket_send_endpoint(&s, client_msg, &ep, 0);
    int read = socket_receive_endpoint(&s, buffer, &ep, 0);

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

  if (is_server) {
    return server();
  } else {
    return client();
  }
}
