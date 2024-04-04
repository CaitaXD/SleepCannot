#ifndef MACROS_H_
#define MACROS_H_

#define NOMINMAX

#define ARRAY_LENGTH(ARRAY) sizeof(ARRAY) / sizeof(ARRAY[0])
#define ARRAY_LITERAL(type, ...) ((type[]){__VA_ARGS__})
#define ARGS_LENGTH(type, ...) (sizeof((type[]){__VA_ARGS__}) / sizeof(type))

#define CONCAT_(A, B) A##B
#define CONCAT2(A, B) CONCAT_(A, B)
#define CONCAT3(A, B, C) CONCAT2(CONCAT2(A, B), C)
#define CONCAT4(A, B, C, D) CONCAT2(CONCAT3(A, B, C), D)
#define CONCAT5(A, B, C, D, E) CONCAT2(CONCAT4(A, B, C, D), E)

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define CLAMP(X, MIN, MAX) ((X) < (MIN) ? (MIN) : ((X) > (MAX) ? (MAX) : (X)))

#endif // MACROS_H_
