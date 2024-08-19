#ifndef MACROS_H_
#define MACROS_H_

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <functional>
#include <string>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <ifaddrs.h>

#define ARRAY_LENGTH(ARRAY) sizeof(ARRAY) / sizeof(ARRAY[0])
#define ARRAY_POSTFIXLEN(ARRAY) ARRAY, ARRAY_LENGTH(ARRAY)
#define ARRAY_PREFIXLEN(ARRAY) ARRAY_LENGTH(ARRAY), ARRAY

#define perrorcode(message) perrorcode_(message, __FILE__, __LINE__)

#define eprintf(message, ...) fprintf(stderr, message " [%s:%d]", __FILE__, __LINE__, ##__VA_ARGS__)

static inline void perrorcode_(const char *message, const char *file, int line)
{
  perror(message);
  std::cerr << "errno: " << errno << " in " << file << ":" << line << std::endl;
}

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

using string = std::string;
using string_view = std::string_view;

static inline string get_hostname()
{
  char hostname[1024];
  gethostname(hostname, 1024 - 1);
  return string(hostname);
}

#ifdef LOG_ENABLE
  #define LOG(message)printf("[LOG] %s [%s:%d]\n", message, __FILE__, __LINE__);
  #define LOGF(fmt, ...) printf("[LOG] " fmt " [%s:%d]" "\n", ## __VA_ARGS__, __FILE__, __LINE__)
#else
  #define LOG(message)
  #define LOGF(fmt, ...)
#endif

#ifdef FUZZ_ENABLE
  #define FUZZ_DELAY \
  do {\
    if (rand() % 10 == 0) \
      {\
        srand(time(NULL));\
        int fuzz_delay = rand() % 5 + 5;\
        LOGF("Sleeping for %d seconds\n", fuzz_delay);\
        sleep(rand() % fuzz_delay);\
        LOG("Woke up\n");\
      }\
  } while (0)
#else
  #define FUZZ_DELAY
#endif

#define RSLEEP_MIN 50
#define RSLEEP_MAX 500
static void rsleep() {
  int rng = rand() % RSLEEP_MAX + RSLEEP_MIN;
  msleep(rng);
}

#endif // MACROS_H_
