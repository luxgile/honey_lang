#include "hstring.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

String *string_new(const char *str) {
  String *s = (String *)malloc(sizeof(String));
  if (s == NULL) {
    return NULL;
  }

  s->len = str == NULL ? 0 : strlen(str);
  s->capacity = s->len > 15 ? s->len + 1 : 16;

  s->ptr = (char *)malloc(s->capacity);
  if (s->ptr == NULL) {
    free(s);
    return NULL;
  }

  if (str != NULL) {
    strcpy(s->ptr, str);
  } else {
    s->ptr[0] = '\0';
  }

  return s;
}

void string_delete(String *s) {
  if (s == NULL)
    return;
  free(s->ptr);
  free(s);
}

void string_grow(String *s, uint new_capacity) {
  if (s == NULL || new_capacity <= s->capacity)
    return;

  char *data = (char *)realloc(s->ptr, new_capacity);
  if (data == NULL)
    return;

  s->ptr = data;
  s->capacity = new_capacity;
}

void string_push(String *s, const char *str) {
  if (s == NULL || str == NULL)
    return;

  uint new_len = strlen(str) + s->len;
  if (new_len > s->capacity) {
    uint new_capacity =
        s->capacity * 2 > new_len ? s->capacity * 2 : new_len + 1;
    string_grow(s, new_capacity);
  }

  strcat(s->ptr, str);
  s->len = new_len;
}

void string_clear(String *s) {
  if (s == NULL)
    return;

  s->len = 0;
  s->ptr = "";
}

