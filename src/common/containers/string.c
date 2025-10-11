#include "string.h"

#include <stdlib.h>
#include <string.h>

static const float growth_factor = 1.5f;

void string_deinit(string_t* str)
{
    if (str->capacity > SMALL_STRING_OPTIMIZATION_SIZE) {
        free(str->heap_mem);
    }

    str->capacity = 0;
    str->length = 0;
}

string_t* string_create()
{
    string_t* str = malloc(sizeof(*str));
    if (str == nullptr) {
        return nullptr;
    }

    *str = STRING_INIT;
    return str;
}

void string_destroy(string_t* str)
{
    if (str == nullptr) {
        return;
    }

    string_deinit(str);
    free(str);
}

const char* string_cstr(string_t* str)
{
    if (str->capacity > SMALL_STRING_OPTIMIZATION_SIZE)
        return str->heap_mem;
    return str->sso_buf;
}

char* string_release(string_t* str)
{
    char* mem = nullptr;
    if (str->capacity > SMALL_STRING_OPTIMIZATION_SIZE)
        mem = str->heap_mem;
    else
        mem = strdup(str->sso_buf);

    str->heap_mem = nullptr;
    str->capacity = SMALL_STRING_OPTIMIZATION_SIZE;
    str->length = 0;

    return mem;
}

size_t string_len(string_t* str)
{
    return str->length;
}

static void string_grow(string_t* str, size_t min_capacity)
{
    if (str->capacity >= min_capacity)
        return;

    // Grow capacity in multiples of the growth-factor
    size_t new_capacity = str->capacity;
    do
    {
        new_capacity = (size_t)(new_capacity * growth_factor);
    } while (new_capacity < min_capacity);

    // Make a new heap-allocated char array with current content
    char* new_heap_mem = malloc(new_capacity);
    if (str->length > 0) {
        const char* src = str->capacity > SMALL_STRING_OPTIMIZATION_SIZE
                        ? str->heap_mem
                        : str->sso_buf;
        memcpy(new_heap_mem, src, str->length);
    }
    new_heap_mem[str->length] = '\0';

    // Update string_t object
    if (str->capacity > SMALL_STRING_OPTIMIZATION_SIZE)
        free(str->heap_mem);
    str->heap_mem = new_heap_mem;
    str->capacity = new_capacity;
}

void string_append_cstr(string_t* str, const char* cstr)
{
    size_t cstr_len = strlen(cstr);
    size_t needed_capacity = str->length + cstr_len + 1;

    if (str->capacity < needed_capacity)
        string_grow(str, needed_capacity);

    char* dest;
    if (str->capacity > SMALL_STRING_OPTIMIZATION_SIZE)
        dest = str->heap_mem + str->length;
    else
        dest = str->sso_buf + str->length;

    memcpy(dest, cstr, cstr_len + 1); // +1 to copy null-terminator
    str->length += cstr_len;
}

void string_append_char(string_t* str, char c)
{
    size_t needed_capacity = str->length + 2;
    if (str->capacity < needed_capacity)
        string_grow(str, needed_capacity);

    char* dest;
    if (str->capacity > SMALL_STRING_OPTIMIZATION_SIZE)
        dest = str->heap_mem + str->length;
    else
        dest = str->sso_buf + str->length;

    *dest = c;
    *(dest + 1) = '\0';
    ++str->length;
}
