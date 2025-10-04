#include "vec.h"

#include <stdlib.h>
#include <string.h>

static int VEC_INITIAL_CAPACITY = 8;
static float VEC_GROWTH_FACTOR = 1.5f;

void vec_deinit(vec_t* vec)
{
    if (vec == nullptr) {
        return;
    }

    free(vec->mem);

    vec->mem = nullptr;
    vec->length = 0;
    vec->capacity = 0;
    vec->elem_size = 0;
}

vec_t* vec_create(size_t elem_size)
{
    vec_t* vec = malloc(sizeof(*vec));
    // TODO: panic if malloc returns nullptr

    vec->mem = nullptr;
    vec->elem_size = elem_size;
    vec->length = 0;
    vec->capacity = 0;

    return vec;
}

void vec_destroy(vec_t* vec)
{
    if (vec == nullptr) {
        return;
    }

    vec_deinit(vec);
    free(vec);
}

static void vec_grow(vec_t* vec)
{
    size_t new_capacity = vec->capacity == 0 ? VEC_INITIAL_CAPACITY : (int)(vec->capacity * VEC_GROWTH_FACTOR);

    void* new_mem = realloc(vec->mem, new_capacity * vec->elem_size);
    // TODO: panic if realloc returns nullptr

    vec->mem = new_mem;
    vec->capacity = new_capacity;
}

void vec_append(vec_t* vec, void* elem)
{
    if (vec->length >= vec->capacity) {
        vec_grow(vec);
    }

    void* dest = (uint8_t*)vec->mem + (vec->length * vec->elem_size);
    memcpy(dest, elem, vec->elem_size);

    ++vec->length;
}

void vec_move(vec_t* dst, vec_t* src)
{
    free(dst->mem);
    *dst = *src;

    src->mem = nullptr;
    src->elem_size = 0;
    src->length = 0;
    src->capacity = 0;
}
