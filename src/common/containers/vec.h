#ifndef CONTAINERS_PTR_VEC__H
#define CONTAINERS_PTR_VEC__H

#include <stdint.h>
#include <stdlib.h>

typedef void (*vec_delete_fn)(void* elem);

typedef struct vec
{
    void** mem;
    size_t length;
    size_t capacity;
    vec_delete_fn delete_fn;
} vec_t;

#define VEC_INIT(del_fn) (vec_t){ \
    .delete_fn = del_fn, \
}

void vec_deinit(vec_t* vec);

vec_t* vec_create(vec_delete_fn delete_fn);

void vec_destroy(vec_t* vec);

void vec_push(vec_t* vec, void* elem);

void* vec_pop(vec_t* vec);

void* vec_replace(vec_t* vec, size_t index, void* elem);

void vec_move(vec_t* dst, vec_t* src);

static inline size_t vec_size(vec_t* vec)
{
    return vec->length;
}

static inline void* vec_get(vec_t* vec, size_t index)
{
    // TODO: panic if index >= length
    return vec->mem[index];
}

static inline void* vec_last(vec_t* vec)
{
    return vec_get(vec, vec_size(vec) - 1);
}

#endif
