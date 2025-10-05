#ifndef CONTAINERS_PTR_VEC__H
#define CONTAINERS_PTR_VEC__H

#include <stdint.h>
#include <stdlib.h>

typedef void (*ptr_vec_elem_destructor_fn)(void* elem);

typedef struct ptr_vec
{
    void* mem;
    size_t length;
    size_t capacity;
} ptr_vec_t;

#define PTR_VEC_INIT (ptr_vec_t){}

void ptr_vec_deinit(ptr_vec_t* vec, ptr_vec_elem_destructor_fn destructor_fn);

ptr_vec_t* ptr_vec_create();

void ptr_vec_destroy(ptr_vec_t* vec, ptr_vec_elem_destructor_fn destructor_fn);

void ptr_vec_append(ptr_vec_t* vec, void* ptr);

void ptr_vec_move(ptr_vec_t* dst, ptr_vec_t* src);

static inline size_t ptr_vec_size(ptr_vec_t* vec)
{
    return vec->length;
}

static inline void* ptr_vec_get(ptr_vec_t* vec, size_t index)
{
    // TODO: panic if index >= length
    return *(void**)((uint8_t*)vec->mem + (index * sizeof(void*)));
}

#endif
