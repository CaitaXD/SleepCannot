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

#define NOMINMAX

#define STRINGYFY(x) #x
#define ARRAY_LENGTH(ARRAY) sizeof(ARRAY) / sizeof(ARRAY[0])
#define ARRAY_LITERAL(type, ...) ((type[]){__VA_ARGS__})
#define ARGS_LENGTH(type, ...) (sizeof((type[]){__VA_ARGS__}) / sizeof(type))
#define UNPACK_ARRAY(ARRAY) ARRAY, ARRAY_LENGTH(ARRAY)

#define SCOPE(VAR) for (VAR, *_once = NULL + 1; _once; _once = NULL)
#define STATEMENT(...)                                                         \
  do {                                                                         \
    __VA_ARGS__                                                                \
  } while (0)

#define CONCAT_(A, B) A##B
#define CONCAT2(A, B) CONCAT_(A, B)
#define CONCAT3(A, B, C) CONCAT2(CONCAT2(A, B), C)
#define CONCAT4(A, B, C, D) CONCAT2(CONCAT3(A, B, C), D)
#define CONCAT5(A, B, C, D, E) CONCAT2(CONCAT4(A, B, C, D), E)

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define CLAMP(X, MIN, MAX) ((X) < (MIN) ? (MIN) : ((X) > (MAX) ? (MAX) : (X)))

#define IS_SAME_TYPE(X, Y) (_Generic((X), typeof(Y) : 1, default : 0))
#define ASSERT_TYPE(X, Y) (assert(IS_SAME_TYPE(X, Y) && "Type Missmatch"))

#define Ref(X) ((typeof(X)[1]){X})

typedef bool (*eq)(void *lhs, void *rhs);
typedef int (*comparer)(void *lhs, void *rhs);
typedef uint32_t (*hashfn)(void *value);

struct Defer {
    Defer(std::function<void()> f) : f(f) {}
    ~Defer() { f(); }
    std::function<void()> &f;
};

#define defer(code) Defer{[&](){code;}}

template <typename TCallable, typename TReturn>
TReturn invoke_callable(TCallable *callable) {
  return (TReturn) (*callable)();
}

template <typename TReturn = void*, typename FunctionPointer = TReturn (*)(void*), typename TCallable>
FunctionPointer to_fnptr(const TCallable &callable) {
  return  (FunctionPointer) invoke_callable<TCallable, TReturn>;
}

static inline void perrorcode(const char *message) {
  fprintf(stderr, "Error Code: %d\n", errno);
  perror(message);
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
  gethostname(hostname, 1024);
  return string(hostname);
}

#endif // MACROS_H_
