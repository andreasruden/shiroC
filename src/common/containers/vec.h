#ifndef CONTAINERS_VEC__H
#define CONTAINERS_VEC__H

#include <stddef.h>
#include <stdint.h>

typedef void (*vec_elem_destructor_fn)(void* elem);

typedef struct vec \
{
    void* mem;
    size_t elem_size;
    size_t length;
    size_t capacity;
} vec_t;

#define VEC_INIT(type) (vec_t){ \
        .elem_size = sizeof(type) \
    }

void vec_deinit(vec_t* vec, vec_elem_destructor_fn elem_destructor);

vec_t* vec_create(size_t elem_size);

void vec_destroy(vec_t* vec, vec_elem_destructor_fn elem_destructor);

void vec_append(vec_t* vec, void* elem);

void vec_move(vec_t* dst, vec_t* src);

static inline size_t vec_size(vec_t* vec)
{
    return vec->length;
}

static inline void* vec_get(vec_t* vec, size_t index)
{
    // TODO: panic if index >= length
    return (uint8_t*)vec->mem + (index * vec->elem_size);
}

#endif
