#ifndef CONTAINERS_PTR_VEC__H
#define CONTAINERS_PTR_VEC__H

#include "vec.h"

#include <stdlib.h>

typedef struct ptr_vec
{
    vec_t inner;
} ptr_vec_t;

#define PTR_VEC_INIT (ptr_vec_t){ \
    .inner = VEC_INIT(void*) \
}

static inline void ptr_vec_deinit(ptr_vec_t* vec)
{
    vec_deinit(&vec->inner);
}

static inline ptr_vec_t* ptr_vec_create(void)
{
    ptr_vec_t* vec = (ptr_vec_t*)malloc(sizeof(ptr_vec_t));
    // TODO: panic if nullptr
    vec->inner = VEC_INIT(void*);
    return vec;
}

static inline void ptr_vec_destroy(ptr_vec_t* vec)
{
    ptr_vec_deinit(vec);
    free(vec);
}

static inline void ptr_vec_append(ptr_vec_t* vec, void* ptr)
{
    vec_append(&vec->inner, &ptr);
}

static inline void* ptr_vec_get(ptr_vec_t* vec, size_t index)
{
    return *(void**)vec_get(&vec->inner, index);
}

static inline size_t ptr_vec_size(ptr_vec_t* vec)
{
    return vec_size(&vec->inner);
}

static inline void ptr_vec_move(ptr_vec_t* dst, ptr_vec_t* src)
{
    vec_move(&dst->inner, &src->inner);
}

#endif
