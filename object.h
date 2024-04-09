#ifndef OBJECT_H_
#define OBJECT_H_

#include "str.h"
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
  HS_CODE,
  METHOD_COUNT,
} ObjectMethodHandle;

typedef struct ObjectVTable {
  Str (*to_string)(Object *obj);
  unsigned int (*hash_code)(Object *obj);
} ObjectMethods;

extern void *ObjectVtbl[METHOD_COUNT];
extern ObjectMethods *Methods;

Str to_string(Object *obj);
unsigned int hash_code(Object *obj);

#endif // OBJECT_H_

#ifdef OBJECT_IMPLEMENTATION

static inline Str object_to_string(Object *obj) {
  (void)(obj);
  return STR("[Object object]");
}

static inline unsigned int object_hash_code(Object *obj) {
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

unsigned int hash_code(Object *obj) {
  void *method = obj->vtbl[HS_CODE];
  return ((unsigned int (*)(Object *))method)(obj);
}

void *ObjectVtbl[METHOD_COUNT] = {
    [TO_STRING] = (void *)object_to_string,
    [HS_CODE] = (void *)object_hash_code,
};

ObjectMethods *Methods = (ObjectMethods *)ObjectVtbl;

#endif // OBJECT_IMPLEMENTATION