#ifndef OBJECT_H_
#define OBJECT_H_

#include "DataStructures/str.h"
#include <stdint.h>
#include <stdint.h>

typedef struct Object {
  union {
    void **vtbl;
    struct ObjectVTable *method;
  };
} Object;

#define Object()                                                               \
  (Object) { .vtbl = ObjectVtbl }

typedef enum ObjectMethodHandle {
  TO_STRING,
  EQUALS,
  HASH_CODE,
  METHOD_COUNT,
} ObjectMethodHandle;

typedef struct ObjectVTable {
  Str (*to_string)(Object *obj);
  bool (*equals)(Object *lhs, Object *rhs);
  uint32_t (*hash_code)(Object *obj);
} ObjectMethods;

extern void *ObjectVtbl[METHOD_COUNT];
extern ObjectMethods *Methods;

Str to_string(Object *obj);
uint32_t hash_code(Object *obj);

#endif // OBJECT_H_

#ifdef OBJECT_IMPLEMENTATION

Str object_to_string(Object *obj) {
  (void)(obj);
  return STR("[Object object]");
}

bool object_equals(Object *lhs, Object *rhs) {
  return lhs == rhs;
}

uint32_t object_hash_code(Object *obj) {
  unsigned int hash = 17;
  for (size_t i = 0; i < sizeof(void *); i++) {
    hash += ((uint8_t *)obj)[i] * 31;
  }
  return hash;
}

Str to_string(Object *obj) {
  void *method = obj->vtbl[TO_STRING];
  return ((Str(*)(Object *))method)(obj);
}

bool equals(Object *lhs, Object *rhs) {
  void *method = lhs->vtbl[EQUALS];
  return ((bool (*)(Object *, Object *))method)(lhs, rhs);
}

uint32_t hash_code(Object *obj) {
  void *method = obj->vtbl[HASH_CODE];
  return ((unsigned int (*)(Object *))method)(obj);
}

void *ObjectVtbl[METHOD_COUNT] = {
    [TO_STRING] = (void *)object_to_string,
    [HASH_CODE] = (void *)object_hash_code,
};

ObjectMethods *Methods = (ObjectMethods *)ObjectVtbl;

#endif // OBJECT_IMPLEMENTATION