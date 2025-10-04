#ifndef CONTAINERS_STRING__H
#define CONTAINERS_STRING__H

#include <stddef.h>

static constexpr int SMALL_STRING_OPTIMIZATION_SIZE = 16;

typedef struct string
{
    union
    {
        char  sso_buf[SMALL_STRING_OPTIMIZATION_SIZE];
        char* heap_mem;
    };
    size_t capacity;
    size_t length;
} string_t;

#define STRING_INIT (string_t){ \
        .capacity = SMALL_STRING_OPTIMIZATION_SIZE \
    }

// Release memory used by string. Must be used on strings not created with string_create().
void string_deinit(string_t* str);

string_t* string_create();

void string_destroy(string_t* str);

const char* string_cstr(string_t* str);

/* Ownership of string is released and given to caller.

   Remarks:
   The string_t object can be reused in the same way a new string_t can be used.
   For strings not created with string_create, there is no need to call string_deinit afterwards.
   For a string_t created with string_create, string_destroy() still needs to be called.
*/
char* string_release(string_t* str);

void string_append_cstr(string_t* str, const char* cstr);

#endif
