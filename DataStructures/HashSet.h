#ifndef HashSet_H_
#define HashSet_H_

#include "../macros.h"
#include <assert.h>
#include <bits/posix1_lim.h>
#include <limits.h>
#include <math.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#if !defined(ALLOC) && !defined(FREE) && !defined(REALLOC)
#include <malloc.h>
#define HS_ALLOC malloc
#define HS_FREE free
#define HS_REALLOC realloc
#elif !defined(ALLOC) || !defined(FREE) || !defined(REALLOC)
#error "You must define all or none of ALLOC, FREE, REALLOC"
#endif

typedef struct Entry {
  ssize_t hash_code;
  ssize_t next;
} Entry;

typedef struct HashSet {
  ssize_t *buckets;
  size_t count;
  size_t size;
  ssize_t free_list;
  ssize_t free_count;
  hashfn hash;
  eq equals;
  Entry *entries;
} HashSet;

#define HashSet(type)                                                          \
  typeof(union {                                                               \
    HashSet base;                                                              \
    struct {                                                                   \
      ssize_t *buckets;                                                        \
      size_t count;                                                            \
      size_t size;                                                             \
      ssize_t free_list;                                                       \
      ssize_t free_count;                                                      \
      hashfn hash;                                                             \
      eq equals;                                                           \
      struct {                                                                 \
        Entry entry;                                                           \
        type value;                                                            \
      } * entries;                                                             \
    };                                                                         \
  })

#define KeyValuePair(Key, Value)                                               \
  typeof(struct {                                                              \
    Key key;                                                                   \
    Value value;                                                               \
  })

#define HashTable(key, value) HashSet(KeyValuePair(key, value))

#define HS_ITEM_TYPE(set) typeof((set).entries->value)
#define HS_ITEM_SIZE(set) sizeof(HS_ITEM_TYPE(set))
#define HT_KEY_TYPE(set) typeof((set).entries->value.key)
#define HT_VALUE_TYPE(set) typeof((set).entries->value.value)
#define HT_KVP_TYPE(set) KeyValuePair(HT_KEY_TYPE(set), HT_VALUE_TYPE(set))

extern int primes_lut[];
extern size_t primes_len;

bool hash_set_add(HashSet *hash_set, void *value, size_t elemnt_size);
void hash_set_resize(HashSet *hash_set, size_t size, size_t element_size,
                     bool force_rehash);
void hash_set_init(HashSet *hash_set, size_t element_size, size_t size_hint);

#define hash_set_add(hash_set, value)                                          \
  (hash_set_add)(&hash_set.base, (void *)Ref(value), sizeof(typeof(value)))

#define hash_table_add(hash_map, key, value)                                   \
  STATEMENT(HT_KVP_TYPE(hash_map) _pair = {key, value};                        \
            hash_set_add(hash_map, _pair);)

#define hash_set_init(hash_set)                                                \
  (hash_set_init)(&hash_set.base, sizeof(typeof(hash_set.items[0])),           \
                  hash_set.count)

#define hash_set_resize(hash_set, size)                                        \
  (hash_set_resize)(&hash_set.base, size, sizeof(typeof(hash_set.items[0])),   \
                    false)

#define hash_set_rehash(hash_set, size)                                        \
  (hash_set_resize)(&hash_set.base, size, sizeof(typeof(hash_set.items[0])),   \
                    true)

typedef struct HashSetIterator {
  HashSet *const hash_set;
  size_t index;
} HashSetIterator;

#define HashSetIterator(type)                                                  \
  typeof(union {                                                               \
    HashSetIterator base;                                                      \
    struct {                                                                   \
      HashSet(type) *const hash_set;                                           \
      size_t index;                                                            \
      type current;                                                            \
    };                                                                         \
  })

#define HashSetGetIterator(set)                                                \
  {                                                                            \
    .base = { &set.base, 0 }                                                   \
  }

#define HS_ITERATOR_TYPE(set) typeof(HashSetIterator(HS_ITEM_TYPE(set)))

bool hash_set_iterator_next(HashSetIterator *iter, void *out_current,
                            size_t element_size);

#define hash_set_iterator_next(iter)                                           \
  (hash_set_iterator_next)(&iter.base, (void *)&iter.current,                  \
                           HS_ITEM_SIZE(*iter.hash_set))

#define hash_set_iterator_current(iter) (iter).current

#define hash_set_foreach(val, hset)                                            \
  SCOPE(HS_ITERATOR_TYPE(hset) it = HashSetGetIterator(hset))                  \
  while (hash_set_iterator_next(it))                                           \
  SCOPE(HS_ITEM_TYPE(hset) val = hash_set_iterator_current(it))

#endif // HashSet_H_

#define HASH_SET_IMPLEMENTATION
#ifdef HASH_SET_IMPLEMENTATION

const int MAX_PRIME_ARRAY_LENGTH = 0x7FEFFFFD;

int primes_lut[] = {
    3,       7,       11,      17,      23,      29,      37,      47,
    59,      71,      89,      107,     131,     163,     197,     239,
    293,     353,     431,     521,     631,     761,     919,     1103,
    1327,    1597,    1931,    2333,    2801,    3371,    4049,    4861,
    5839,    7013,    8419,    10103,   12143,   14591,   17519,   21023,
    25229,   30293,   36353,   43627,   52361,   62851,   75431,   90523,
    108631,  130363,  156437,  187751,  225307,  270371,  324449,  389357,
    467237,  560689,  672827,  807403,  968897,  1162687, 1395263, 1674319,
    2009191, 2411033, 2893249, 3471899, 4166287, 4999559, 5999471, 7199369};

size_t primes_len = (sizeof(primes_lut) / sizeof(primes_lut[0]));
const int HS_PRIME = 101;

bool is_prime(ssize_t candidate) {
  if ((candidate & 1) != 0) {
    ssize_t limit = sqrt(candidate);
    for (ssize_t divisor = 3; divisor <= limit; divisor += 2) {
      if ((candidate % divisor) == 0)
        return false;
    }
    return true;
  }
  return candidate == 2;
}

int get_prime(ssize_t min) {
  assert(min >= 0 && "overflow");

  for (size_t i = 0; i < primes_len; i++) {
    ssize_t prime = primes_lut[i];
    if (prime >= min)
      return prime;
  }

  for (ssize_t i = (min | 1); i < SSIZE_MAX; i += 2) {
    if (is_prime(i) && ((i - 1) % HS_PRIME != 0))
      return i;
  }
  return min;
}

ssize_t expand_prime(int oldSize) {
  int newSize = 2 * oldSize;

  if ((size_t)newSize > MAX_PRIME_ARRAY_LENGTH &&
      MAX_PRIME_ARRAY_LENGTH > oldSize) {
    assert(MAX_PRIME_ARRAY_LENGTH == get_prime(MAX_PRIME_ARRAY_LENGTH) &&
           "Invalid MAX_PRIME_ARRAY_LENGTH");
    return MAX_PRIME_ARRAY_LENGTH;
  }

  return get_prime(newSize);
}

#define ALIGNED(alignment, n) (((n) + ((alignment)-1)) & ~((alignment)-1))
#define ENTRY_SIZE(element_size)                                               \
  ALIGNED(alignof(Entry), sizeof(Entry) + element_size)

Entry *_entry_at(HashSet *hash_set, size_t element_size, size_t index) {
  intptr_t entries = (intptr_t)hash_set->entries;
  size_t aligned_size = ENTRY_SIZE(element_size);
  return (Entry *)(entries + index * aligned_size);
}

void *_entry_value(Entry *entry) {
  intptr_t value = (intptr_t)entry;
  return (void *)(value + (sizeof(Entry)));
}

void(hash_set_init)(HashSet *hash_set, size_t element_size, size_t size_hint) {
  size_t size = get_prime(size_hint);
  if (size < hash_set->size) {
    return;
  }
  hash_set->size = size;

  size_t buckets_in_bytes = size * sizeof(ssize_t);
  hash_set->buckets = (ssize_t *)HS_ALLOC(buckets_in_bytes);
  assert(hash_set->buckets && "out of memory");
  memset(hash_set->buckets, 0, buckets_in_bytes);

  size_t aligned_size = ENTRY_SIZE(element_size);
  size_t entires_in_bytes = (size * aligned_size);
  hash_set->entries = (Entry *)HS_ALLOC(entires_in_bytes);
  assert(hash_set->entries && "out of memory");
  memset(hash_set->entries, 0, entires_in_bytes);

  hash_set->count = 0;
  hash_set->free_list = -1;
  hash_set->free_count = 0;
}

void(hash_set_resize)(HashSet *hash_set, size_t size, size_t element_size,
                      bool force_rehash) {

  size_t aligned_size = ENTRY_SIZE(element_size);
  size_t old_size = hash_set->size * aligned_size;
  size_t new_size = size * aligned_size;
  intptr_t entries = (intptr_t)HS_REALLOC(hash_set->entries, new_size);
  memset((void *)(entries + old_size), 0, new_size - old_size);

  hash_set->entries = (Entry *)entries;
  hash_set->size = size;
  ssize_t count = hash_set->count;
  if (force_rehash) {
    eq eq = hash_set->equals;
    hashfn hash = hash_set->hash;
    for (ssize_t i = 0; i < count; ++i) {
      Entry *entry = _entry_at(hash_set, element_size, i);
      void *entry_value = _entry_value(entry);
      if (entry->next >= -1) {
        entry->hash_code = entry_value ? hash(entry_value) : 0;

        ssize_t *bucket = hash_set->buckets + (entry->hash_code % size);
      }
    }
  }

  hash_set->buckets = (ssize_t *)HS_ALLOC(size * sizeof(ssize_t));
  for (ssize_t i = 0; i < count; ++i) {
    Entry *entry = _entry_at(hash_set, element_size, i);
    if (entry->next >= -1) {
      ssize_t *bucket = hash_set->buckets + (entry->hash_code % size);
      entry->next = *bucket - 1; // 1-based index
      *bucket = i + 1;
    }
  }
}

bool(hash_set_add)(HashSet *hash_set, void *value, size_t element_size) {
  ssize_t out_location;
  if (hash_set->buckets == NULL) {
    (hash_set_init)(hash_set, element_size, 0);
  }

  size_t size = hash_set->size;
  Entry *entries = hash_set->entries;
  hashfn hash = hash_set->hash;
  eq eq = hash_set->equals;
  uint32_t collisions = 0;
  uint32_t hash_code = value ? hash(value) : 0;
  ssize_t *bucket = hash_set->buckets + (hash_code % size);
  int i = *bucket - 1; // 1-based index

  while (i >= 0) {
    Entry *entry = _entry_at(hash_set, element_size, i);
    void *entry_value = _entry_value(entry);
    bool hash_equals = entry->hash_code == hash_code;
    bool val_equals = eq(entry_value, value);
    if (hash_equals && val_equals) {
      out_location = i;
      return false;
    }
    i = entry->next;
    collisions++;
    assert(collisions <= size && "Ilegal state, Possible race condition");
  }
  ssize_t index;
  if (hash_set->free_count > 0) {
    index = hash_set->free_list;
    hash_set->free_count--;
    hash_set->free_list =
        _entry_at(hash_set, element_size, hash_set->free_list)->next;
  } else {
    int count = hash_set->count;
    if (count == hash_set->size) {
      (hash_set_resize)(hash_set, expand_prime(count), element_size, false);
      bucket = hash_set->buckets + (hash_code % size);
    }
    index = count;
    hash_set->count = count + 1;
  }

  {
    Entry *entry = _entry_at(hash_set, element_size, index);
    void *entry_value = _entry_value(entry);
    entry->hash_code = hash_code;
    entry->next = *bucket - 1; // 1-based index
    memcpy(entry_value, value, element_size);
    *bucket = index + 1;
    out_location = index;
  }

  return true;
}

bool(hash_set_iterator_next)(HashSetIterator *const iter, void *out_current,
                             size_t element_size) {
  HashSet *const hash_set = iter->hash_set;
  while (iter->index < hash_set->count) {
    Entry *entry = _entry_at(hash_set, element_size, iter->index++);
    if (entry->next >= -1) {
      memcpy(out_current, _entry_value(entry), element_size);
      return true;
    }
  }
  iter->index = hash_set->count + 1;
  memset(out_current, 0, element_size);
  return false;
}

#endif // HASH_SET_IMPLEMENTATION