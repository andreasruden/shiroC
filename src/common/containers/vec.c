#include "vec.h"

#include <stdlib.h>
#include <string.h>

static size_t PTR_VEC_INITIAL_CAPACITY = 8;
static float PTR_VEC_GROWTH_FACTOR = 1.5f;

void vec_deinit(vec_t* vec)
{
    if (vec == nullptr)
        return;

    if (vec->delete_fn != nullptr)
    {
        for (size_t i = 0; i < vec_size(vec); ++i)
            vec->delete_fn(vec_get(vec, i));
    }

    free(vec->mem);

    vec->mem = nullptr;
    vec->length = 0;
    vec->capacity = 0;
}

vec_t* vec_create(vec_delete_fn delete_fn)
{
    vec_t* vec = malloc(sizeof(*vec));
    // TODO: panic if malloc returns nullptr

    *vec = VEC_INIT(delete_fn);

    return vec;
}

void vec_destroy(vec_t* vec)
{
    if (vec == nullptr)
        return;

    vec_deinit(vec);
    free(vec);
}

static void ptr_vec_grow(vec_t* vec)
{
    size_t new_capacity = vec->capacity == 0 ?
        PTR_VEC_INITIAL_CAPACITY : (size_t)(vec->capacity * PTR_VEC_GROWTH_FACTOR);

    void* new_mem = realloc(vec->mem, new_capacity * sizeof(void*));
    // TODO: panic if realloc returns nullptr

    vec->mem = new_mem;
    vec->capacity = new_capacity;
}

void vec_push(vec_t* vec, void* elem)
{
    if (vec->length >= vec->capacity)
        ptr_vec_grow(vec);

    vec->mem[vec->length++] = elem;
}

void* vec_pop(vec_t* vec)
{
    if (vec->length == 0)
        return nullptr;  // TODO: Maybe panic if length == 0?

    return vec->mem[--vec->length];
}

void vec_move(vec_t* dst, vec_t* src)
{
    free(dst->mem);
    *dst = *src;

    src->mem = nullptr;
    src->length = 0;
    src->capacity = 0;
}
