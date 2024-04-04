#include "DynamicArray.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define STR_IMPLEMENTATION
#include "str.h"
#undef STR_IMPLEMENTATION

#undef SOCKET_IMPLEMENTATION
#define SOCKET_IMPLEMENTATION
#include "Sockets/Sockets.h"

#define PORT 25565
#define MAXLINE 1024

int server() {
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
    int read = socket_receive_endpoint(&s, buffer, &ep, 0);
    socket_send_endpoint(&s, hello, &ep, 0);

    printf("Client : " str_fmt "\n", str_args(str_take(buffer, read)));
    if (s.error) {
      printf("Error: %zu\n", s.error);
      perror("Error");
      exit(EXIT_FAILURE);
    }
  }

  return 0;
}

int client() {
  Socket s = socket_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  EndPoint ep = ip_endpoint(INADDR_ANY, PORT);

  Str buffer = STR((char[MAXLINE]){});
  while (1) {

    Str hello = STR("General, Kenoby, you are a bold one");
    socket_send_endpoint(&s, hello, &ep, 0);
    int read = socket_receive_endpoint(&s, buffer, &ep, 0);

    if (s.error) {
      printf("Error: %zu\n", s.error);
      perror("Error");
      exit(EXIT_FAILURE);
    }

    printf("Server: " str_fmt "\n", str_args(str_take(buffer, read)));
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
