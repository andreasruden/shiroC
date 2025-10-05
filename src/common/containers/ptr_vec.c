#include "ptr_vec.h"

#include <stdlib.h>
#include <string.h>

static size_t PTR_VEC_INITIAL_CAPACITY = 8;
static float PTR_VEC_GROWTH_FACTOR = 1.5f;

void ptr_vec_deinit(ptr_vec_t* vec, ptr_vec_elem_destructor_fn elem_destructor)
{
    if (vec == nullptr) {
        return;
    }

    if (elem_destructor != nullptr)
    {
        for (size_t i = 0; i < ptr_vec_size(vec); ++i)
            elem_destructor(ptr_vec_get(vec, i));
    }

    free(vec->mem);

    vec->mem = nullptr;
    vec->length = 0;
    vec->capacity = 0;
}

ptr_vec_t* ptr_vec_create()
{
    ptr_vec_t* vec = malloc(sizeof(*vec));
    // TODO: panic if malloc returns nullptr

    *vec = PTR_VEC_INIT;

    return vec;
}

void ptr_vec_destroy(ptr_vec_t* vec, ptr_vec_elem_destructor_fn elem_destructor)
{
    if (vec == nullptr) {
        return;
    }

    ptr_vec_deinit(vec, elem_destructor);
    free(vec);
}

static void ptr_vec_grow(ptr_vec_t* vec)
{
    size_t new_capacity = vec->capacity == 0 ?
        PTR_VEC_INITIAL_CAPACITY : (size_t)(vec->capacity * PTR_VEC_GROWTH_FACTOR);

    void* new_mem = realloc(vec->mem, new_capacity * sizeof(void*));
    // TODO: panic if realloc returns nullptr

    vec->mem = new_mem;
    vec->capacity = new_capacity;
}

void ptr_vec_append(ptr_vec_t* vec, void* elem)
{
    if (vec->length >= vec->capacity) {
        ptr_vec_grow(vec);
    }

    void* dest = (uint8_t*)vec->mem + (vec->length * sizeof(void*));
    memcpy(dest, &elem, sizeof(void*));

    ++vec->length;
}

void ptr_vec_move(ptr_vec_t* dst, ptr_vec_t* src)
{
    free(dst->mem);
    *dst = *src;

    src->mem = nullptr;
    src->length = 0;
    src->capacity = 0;
}
