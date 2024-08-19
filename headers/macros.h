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

#define ARRAY_POSTFIXLEN(ARRAY) ARRAY, ARRAY_LENGTH(ARRAY)
#define ARRAY_PREFIXLEN(ARRAY) ARRAY_LENGTH(ARRAY), ARRAY
#define ARRAY_LENGTH(ARRAY) sizeof(ARRAY) / sizeof(ARRAY[0])

static inline void perrorcode(const char *message)
{
  perror(message);
  std::cerr << " errno: " << errno << std::endl;
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

#endif // MACROS_H_
