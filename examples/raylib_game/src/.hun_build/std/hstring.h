#pragma once

#include <sys/types.h>

typedef struct String {
  char* ptr;
  uint len;
  uint capacity;
} String;

String *string_new(const char* str);
void string_delete(String *s);
void string_grow(String *s, uint new_capacity); 
void string_push(String *s, const char *str);
void string_clear(String *s);
