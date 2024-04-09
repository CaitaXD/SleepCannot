#ifndef _DYNAMICARRAY_H
#define _DYNAMICARRAY_H

#include "../macros.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#if !defined(ALLOC) && !defined(FREE) && !defined(REALLOC)
#define DYN_ALLOC malloc
#define DYN_FREE free
#define DYN_REALLOC realloc
#elif !defined(ALLOC) || !defined(FREE) || !defined(REALLOC)
#error "You must define all or none of ALLOC, FREE, REALLOC"
#endif

typedef struct DynamicArray {
  uint8_t *items;
  size_t count;
  size_t capacity;
} DynamicArray;

#define DynamicArray(type)                                                     \
  union {                                                                      \
    DynamicArray base;                                                         \
    struct {                                                                   \
      type *items;                                                             \
      size_t count;                                                            \
      size_t capacity;                                                         \
    };                                                                         \
  }

#define DYN_ITEM_TYPE(arr) typeof((arr).items[0])

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
  static_assert(sizeof((array).items[0]) >= sizeof(typeof(value)),             \
                "Element type cannot exceed array element size")

#define assert_non_empty(array) assert((array).count > 0 && "Array is empty")

#define element_size(array) (sizeof((array).items[0]))

ssize_t _dynamic_array_index_of(DynamicArray array, void *item,
                                comparer comparer);

#define dynamic_array_reserve(array, min)                                      \
  if ((min) > (array).capacity) {                                              \
    size_t new_capacity = (array).capacity = calculate_new_capacity(array);    \
    size_t new_size = new_capacity * element_size(array);                      \
    (array).items = DYN_REALLOC((array).items, new_size);                      \
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
  (DYN_FREE((array).items), (array).items = NULL, (array).count = 0,           \
   (array).capacity = 0)

#define CAST(value, TO) *(TO *)(&value)

#define dynamic_array_for(index, array)                                        \
  for (size_t index = 0, _len = (array).count; index < _len; ++index)

#define dynamic_array_at(array, index) ((array).items + index)

#define dynamic_array_empty(array) (array.count == 0)

#define dynamic_array_contains(array, item, cmp)                               \
  (_dynamic_array_index_of(CAST(array, DynamicArray), (void *)&(item),         \
                           (comparer)cmp) != -1)

typedef struct DynamicArrayIterator {
  size_t index;
  DynamicArray *const array;
} DynamicArrayIterator;

#define DynamicArrayIterator(type)                                             \
  union {                                                                      \
    DynamicArrayIterator base;                                                 \
    struct {                                                                   \
      size_t index;                                                            \
      DynamicArray(type) *const array;                                         \
    };                                                                         \
  }

#define DynamicArrayGetIterator(array)                                         \
  {                                                                            \
    .base = {.index = -1, .array = &array.base }                               \
  }

#define DYN_ITERATOR_TPYE(arr) typeof(DynamicArrayIterator(DYN_ITEM_TYPE(arr)))

#define dynamic_array_iterator_next(iterator)                                  \
  ++(iterator).index < (iterator).array->count

#define dynamic_array_iterator_current(iterator)                               \
  (iterator).array->items[(iterator).index]

#define dynamic_array_foreach(val, arr)                                        \
  SCOPE(DYN_ITERATOR_TPYE(arr) it = DynamicArrayGetIterator(arr))          \
  while (dynamic_array_iterator_next(it))                                      \
  SCOPE(DYN_ITEM_TYPE(arr) val = dynamic_array_iterator_current(it))

#endif // _DYNAMICARRAY_H

#ifdef DYNAMIC_ARRAY_IMPLEMENTATION

ssize_t _dynamic_array_index_of(DynamicArray array, void *item,
                                comparer comparer) {
  for (size_t i = 0; i < array.count; ++i) {
    if (comparer(dynamic_array_at(array, i), item) == 0) {
      return i;
    }
  }
  return -1;
}

#endif // DYNAMIC_ARRAY_IMPLEMENTATION