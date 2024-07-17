#ifndef MACROS_H_
#define MACROS_H_

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>


#define NOMINMAX

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

struct defer_t {
    defer_t(std::function<void()> f) : f(f) {}
    ~defer_t() { f(); }
    std::function<void()> f;
};

#define defer(code) defer_t{[&](){code;}}

#endif // MACROS_H_
