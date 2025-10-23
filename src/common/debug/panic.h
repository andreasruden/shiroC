#ifndef DEBUG_PANIC__H
#define DEBUG_PANIC__H

#include <stdio.h>

#define panic(msg, ...) do { \
    fprintf(stderr, "PANIC at %s:%d in %s(): " msg "\n", \
            __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    fflush(stderr); \
    __builtin_trap(); \
} while(0)

#define panic_if(cond) do { \
    if (cond) { \
        fprintf(stderr, "PANIC at %s:%d in %s(): assertion failed because %s\n", \
                __FILE__, __LINE__, __func__, #cond); \
        fflush(stderr); \
        __builtin_trap(); \
    } \
} while(0)

#endif
