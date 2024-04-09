#ifndef STR_H_
#define STR_H_

#include "../macros.h"
#include <assert.h>
#include <ctype.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

typedef struct Str {
  char *data;
  const size_t len;
} Str;

const char *str_to_cstr(Str s);

#define CSTR_LEN(s) (s.data[0] == '\0' ? s.len : s.len + 1)

#define STR(s) ((Str){s, ARRAY_LENGTH(s) - 1})

#define str_to_cstr(s) (_str_to_cstr(s, alloca(CSTR_LEN(s)), CSTR_LEN(s)))

#define str_unpack(s) (s).data, (s).len

#define str_fmt "%.*s"
#define str_args(s) (int)((s).len), (s).data

#define str_foreach(V, S)                                                      \
  for (size_t __it = 0, __end = S.len; __it < __end;)                          \
    for (char V = S.data[__it]; __it < __end; V = S.data[++__it])

Str str_slice(Str s, ssize_t offset, ssize_t count);
Str str_skip(Str s, ssize_t count);
Str str_take(Str s, ssize_t count);
Str str_trim(Str s);
int str_cmp(Str a, Str b);
int str_index_of(Str s, Str sub);
int str_last_index_of(Str s, Str sub);
bool str_starts_with(Str s, Str prefix);
void println(Str s);

#endif // STR_H_

#ifdef STR_IMPLEMENTATION

const char *_str_to_cstr(Str s, char *buff, int buff_size) {
  assert((size_t)buff_size >= CSTR_LEN(s));
  memcpy(buff, s.data, s.len);
  if (buff[s.len] != '\0') {
    buff[s.len] = '\0';
  }
  return buff;
}

Str str_skip(Str s, ssize_t count) {
  if (count < 0) {
    count = s.len - count;
  }
  assert((size_t)count <= s.len);
  return (Str){.data = s.data + count, .len = s.len - count};
}

Str str_take(Str s, ssize_t count) {
  if (count < 0) {
    count = s.len - count;
  }
  assert((size_t)count <= s.len);
  return (Str){.data = s.data, .len = (size_t)count};
}

Str str_slice(Str s, ssize_t offset, ssize_t count) {
  if (offset < 0) {
    offset = s.len - offset;
  }
  if (count < 0) {
    count = s.len - count;
  }
  assert((size_t)offset <= s.len);
  assert((size_t)count <= (s.len - offset));
  return (Str){.data = s.data + offset, .len = (size_t)count};
}

int str_cmp(Str a, Str b) {
  if (a.len != b.len) {
    return a.len - b.len;
  }
  return memcmp(a.data, b.data, a.len);
}

int str_index_of(Str s, Str sub) {

  if (sub.len == 0) {
    return 0;
  }
  if (sub.len > s.len) {
    return -1;
  }
  for (size_t i = 0; i <= s.len - sub.len; i++) {
    if (memcmp(s.data + i, sub.data, sub.len) == 0) {
      return i;
    }
  }
  return -1;
}

int str_last_index_of(Str s, Str sub) {
  if (sub.len == 0) {
    return s.len;
  }
  if (sub.len > s.len) {
    return -1;
  }
  for (ssize_t i = s.len - sub.len; i >= 0; i--) {
    if (memcmp(s.data + i, sub.data, sub.len) == 0) {
      return i;
    }
  }
  return -1;
}

bool str_starts_with(Str s, Str prefix) {
  if (prefix.len > s.len) {
    return false;
  }
  return memcmp(s.data, prefix.data, prefix.len) == 0;
}

Str str_trim(Str s) {
  char *data = (char *)s.data;
  size_t len = s.len;

  while (len > 0 && isspace(data[0])) {
    data++;
    len--;
  }
  while (len > 0 && isspace(data[len - 1])) {
    len--;
  }
  return (Str){.data = data, .len = len};
}

void println(Str s) { printf(str_fmt "\n", str_args(s)); }

#endif // STR_IMPLEMENTATION