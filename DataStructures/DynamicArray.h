#ifndef _DYNAMICARRAY_H
#define _DYNAMICARRAY_H

#include "../macros.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifndef DYNAMIC_ARRAY_API
#define DYNAMIC_ARRAY_API
#endif

#if !defined(ALLOC) && !defined(FREE) && !defined(REALLOC)
#define DYNAMIC_ALLOC malloc
#define DYNAMIC_FREE free
#define DYNAMIC_REALLOC realloc
#elif !defined(ALLOC) || !defined(FREE) || !defined(REALLOC)
#error "You must define all or none of ALLOC, FREE, REALLOC"
#endif

#define DynamicArray(type)                                                     \
  struct {                                                                     \
    type *items;                                                               \
    size_t count;                                                              \
    size_t capacity;                                                           \
  }

#define dynamic_array_from(array)                                              \
  {                                                                            \
    .items = array, .count = ARRAY_LENGTH(array),                              \
    .capacity = ARRAY_LENGTH(array)                                            \
  }

#define STATEMENT(MACRO)                                                       \
  do {                                                                         \
    MACRO                                                                      \
  } while (0)

#define calculate_new_capacity(array)                                          \
  (((array).capacity == 0) ? 2 : ((array).capacity) * 2)

#define assert_compatible_types(array, value)                                  \
  static_assert(sizeof((array).items[0]) >= sizeof((value)),                   \
                "Element type cannot exceed array element size")

#define assert_non_empty(array) assert((array).count > 0 && "Array is empty")

#define element_size(array) (sizeof((array).items[0]))

#define dynamic_array_reserve(array, min)                                      \
  if ((min) > (array).capacity) {                                              \
    size_t new_capacity = (array).capacity = calculate_new_capacity(array);    \
    size_t new_size = new_capacity * element_size(array);                      \
    (array).items = DYNAMIC_REALLOC((array).items, new_size);                  \
    assert((array).items && "out of memory");                                  \
  }

#define dynamic_array_push(array, value)                                       \
  STATEMENT(assert_compatible_types((array), (value));                         \
            dynamic_array_reserve((array), (array).count + 1);                 \
            (array).items[(array).count++] = (value);)

#define dynamic_array_pop(array)                                               \
  (assert_non_empty(array), ((array).items[--(array).count]))

#define dynamic_array_clear(array) ((array).count = 0)

#define dynamic_array_free(array)                                              \
  (DYNAMIC_FREE((array).items), (array).items = NULL, (array).count = 0,       \
   (array).capacity = 0)

#define dynamic_array_at(array, index) ((array).items + (index))

#define dynamic_array_empty(array) ((array).count == 0)

#define dynamic_array_foreach(type, val, array)                                \
  for (size_t _i = 0, _len = (array).count; _i < _len;)                         \
         for(type val = *dynamic_array_at(array, _i); _i < _len; val = * dynamic_array_at(array, ++_i))

typedef DynamicArray(uint8_t) DynamicArray;

#define dynamic_array_contains(array, item)                                    \
  (_dynamic_array_index_of(*(DynamicArray *)&(array), (void *)&(item),         \
                           sizeof(item)) != -1)

// ssize_t _dynamic_array_index_of(DynamicArray array, void *item, size_t
// element_size);
static inline ssize_t _dynamic_array_index_of(DynamicArray array, void *item,
                                              size_t element_size) {
  for (size_t i = 0; i < array.count; ++i) {
    if (memcmp(dynamic_array_at(array, i), (char *)item, element_size) == 0) {
      return i;
    }
  }
  return -1;
}

#endif // _DYNAMICARRAY_H
#undef DYNAMIC_ARRAY_API
