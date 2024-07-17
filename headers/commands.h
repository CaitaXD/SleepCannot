#ifndef COMMANDS_H_
#define COMMANDS_H_

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

using string_view = std::string_view;
using string = std::string;

string client_msg = "General, Kenoby, you are a bold one";
string server_msg = "Hello there!";

typedef struct
{
  unsigned char mac_addr[MAC_ADDR_MAX];
  char mac_str[MAC_STR_MAX];
} MacAddress;

// Represents a participant using the service
typedef struct participant_t
{
  string hostname;
  MacAddress mac;
  string ip;
  bool status; // true means awake, false means asleep
} participant_t;

// Represents the table of users using the service
typedef std::unordered_map<string, participant_t> ParticipantTable;

struct MachineEndpoint : IpEndPoint
{
  MacAddress mac;
  string hostname;

  MachineEndpoint() : IpEndPoint() {}
  MachineEndpoint(in_addr_t address, int port) : IpEndPoint(address, port) {}
  MachineEndpoint(struct sockaddr_in addr_in) : IpEndPoint(addr_in) {}

  MachineEndpoint with_port(int port) const
  {
    MachineEndpoint ep = *this;
    ep.addr_in.sin_port = htons(port);
    return ep;
  }

  MachineEndpoint with_address(uint32_t address) const
  {
    MachineEndpoint ep = *this;
    ep.addr_in.sin_addr.s_addr = htonl(address);
    return ep;
  }

  int get_port() const
  {
    return ntohs(((struct sockaddr_in *)&addr)->sin_port);
  }

  string to_string() const
  {
    auto ip = inet_ntoa(((struct sockaddr_in *)&addr)->sin_addr);
    auto port = ntohs(((struct sockaddr_in *)&addr)->sin_port);
    return hostname + " " + mac.mac_str + " " + string(ip) + ":" + std::to_string(port);
  }
};

typedef std::vector<MachineEndpoint> Clients;

typedef void *(*Callback)(void *);
typedef struct Command
{
  string_view cmd;
  string_view description;
  string_view fmt;
  const Callback callback;
} Command;

#define STRINGYFY(x) #x
enum CommandType
{
  COMMAND_ERROR = -1,
  COMMAND_WAKE_ON_LAN,
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

static inline int msleep(long msec)
{
  struct timespec ts;
  int res;
  if (msec < 0)
  {
    errno = EINVAL;
    return -1;
  }
  ts.tv_sec = msec / 1000;
  ts.tv_nsec = (msec % 1000) * 1000000;
  do
  {
    res = nanosleep(&ts, &ts);
  } while (res && errno == EINTR);
  return res;
}

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

int command_exec(ParticipantTable &participants, std::mutex &mutex)
{
  int exit_code = 0;
  char buffer[MAXLINE];
  fgets(buffer, MAXLINE, stdin);
  ssize_t read = strlen(buffer);
  string cmd = string(buffer).substr(0, read);
  trim(cmd);
  enum CommandType cmd_type = get_command_type(cmd);
  // void *args = NULL;
  std::lock_guard<std::mutex> lock(mutex);
  switch (cmd_type)
  {
  case COMMAND_WAKE_ON_LAN:
  {
    string cmd_args = string(cmd).substr(commands[cmd_type].cmd.size());
    trim(cmd_args);
    auto host_name = string(cmd_args).substr(0, cmd_args.find(" "));
    auto participant = participants.at(host_name);
    string magic_packet = "wakeup " + host_name;
    IpEndPoint broadcast = IpEndPoint::broadcast(INITIAL_PORT + 2);
    Socket s{};
    s = s.open(AdressFamily::InterNetwork, SocketType::Datagram, SocketProtocol::UDP);
    if (s.bind(IpEndPoint{INADDR_ANY, INITIAL_PORT + 2}) < 0)
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
  std::lock_guard<std::mutex> unlock(mutex);
  return exit_code;
}

MacAddress get_mac()
{
  MacAddress mac = {};

  FILE *f = fopen(MAC_ADDRES_FILE, "r");
  if (f == NULL)
  {
    perror("get_mac");
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