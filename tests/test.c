#include "macros.h"
#include <stddef.h>
#define OBJECT_IMPLEMENTATION

#define HASH_SET_IMPLEMENTATION
#include "DataStructures/HashSet.h"
#undef HASH_SET_IMPLEMENTATION

#define DYNAMIC_ARRAY_IMPLEMENTATION
#include "DataStructures/DynamicArray.h"
#undef DYNAMIC_ARRAY_IMPLEMENTATION

#include "object.h"
#include "stdio.h"
#include "stdlib.h"


uint32_t hash_int(int *key) { return *key * 17 + 31; }
int compare_int(int *a, int *b) { return *a - *b; }

uint32_t hash_string(const char *string) {
  uint32_t h = 0;
  while (*string) {
    h = 31 * h + *string++;
  }
  return h;
}

typedef HashSet(int) HashSetInt;
typedef HashTable(char *, int) HashTableStringInt;

#define HashSetInt()                                                           \
  { .hash = (hashfn)hash_int, .equals = (eq)compare_int }

#define HashSetString()                                                        \
  { .hash = (hashfn)hash_string, .equals = (eq)object_equals }

#define HashTableStringInt()                                                   \
  { .hash = (hashfn)object_hash_code, .equals = (eq)object_equals }

int main() {

  HashSet(char*) hm = HashSetString();
  
  hash_set_add(hm, "bob");
  hash_set_add(hm, "McBob");
  hash_set_add(hm, "the bob");
  hash_set_add(hm, "bobson");


  hash_set_foreach(kvp, hm) { printf("%s\n", kvp); }

  HashSet(int) hs = HashSetInt();
  hash_set_add(hs, 1);
  hash_set_add(hs, 2);
  hash_set_add(hs, 3);

  hash_set_foreach(kvp, hs) { printf("%d\n", kvp); }


  return 0;
}