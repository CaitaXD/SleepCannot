

#include <stdalign.h>
#define HASH_SET_IMPLEMENTATION
#include "DataStructures/HashSet.h"
#undef HASH_SET_IMPLEMENTATION

#define DYNAMIC_ARRAY_IMPLEMENTATION
#include "DataStructures/DynamicArray.h"
#undef DYNAMIC_ARRAY_IMPLEMENTATION

#include "stdio.h"
#include "stdlib.h"

uint32_t hash_int(int *key) { return *key * 17 + 31; }
int compare_int(int *a, int *b) { return *a - *b; }

uint32_t hash_string(const char *key) {
  const char *p = key;
  uint32_t h = 0;
  while (*p) {
    h = 31 * h + *p++;
  }
  return h;
}

typedef HashSet(int) HashSetInt;
typedef HashTable(char *, int) HashTableStringInt;

#define HashSetInt()                                                           \
  (HashSetInt) { .hash = (hashfn)hash_int, .compare = (comparer)compare_int }

#define HashTableStringInt()                                                   \
  (HashTableStringInt) {                                                       \
    .hash = (hashfn)hash_string, .compare = (comparer)strcmp                   \
  }

int main() {
  HashSetInt set = HashSetInt();
  hash_set_add(set, 0);
  hash_set_add(set, 1);
  hash_set_add(set, 1);
  hash_set_add(set, 2);
  hash_set_add(set, 3);
  hash_set_add(set, 4);
  hash_set_add(set, 4);
  hash_set_add(set, 5);
  hash_set_add(set, 6);
  hash_set_add(set, 7);
  hash_set_add(set, 8);
  hash_set_add(set, 9);
  hash_set_add(set, 9);
  hash_set_add(set, 9);
  hash_set_add(set, 10);
  hash_set_add(set, 10);

  // for (ssize_t i = 0; i < set.count;) {
  //   Entry *entry = _entry_at((HashSet *)(&set), sizeof(int), i);
  //   int integer = *(int *)_entry_value(entry);
  //   if (entry->next >= -1) {
  //     printf("%d\n", integer);
  //   }
  //   entry = _entry_at((HashSet *)(&set), sizeof(int), ++i);
  // }

  printf("HashSet Explicit iterator\n");
  typeof(HashSetIterator(int)) iter = HashSetGetIterator(set);
  while (hash_set_iterator_next(iter)) {
    printf("%d, ", iter.current);
  }
  printf("\n");

  printf("HashSet Implicit iterator\n");
  hash_set_foreach(i32, set) { printf("%d, ", i32); }
  printf("\n");


  typeof(DynamicArray(int)) array = {};
  dynamic_array_push(array, 0);
  dynamic_array_push(array, 1);
  dynamic_array_push(array, 2);
  dynamic_array_push(array, 3);
  dynamic_array_push(array, 4);
  dynamic_array_push(array, 5);
  dynamic_array_push(array, 6);
  dynamic_array_push(array, 7);
  dynamic_array_push(array, 8);
  dynamic_array_push(array, 9);
  dynamic_array_push(array, 10);

  printf("DynamicArray Explicit iterator\n");
  typeof(DynamicArrayIterator(int)) da_iter = DynamicArrayGetIterator(array);
  while (dynamic_array_iterator_next(da_iter)) {
    printf("%d, ", dynamic_array_iterator_current(da_iter));
  }
  printf("\n");

  printf("DynamicArray Implicit iterator\n");
  dynamic_array_foreach(i32, array) { printf("%d, ", i32); }
  printf("\n");

  return 0;
}