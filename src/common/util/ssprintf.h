#include <alloca.h>
#include <stddef.h>
#include <stdio.h>

#define ssprintf(fmt, ...) ({ \
    int ssprintf_needed_length_ = snprintf(nullptr, 0, fmt __VA_OPT__(,) __VA_ARGS__) + 1; \
    char* ssprintf_buffer_ = alloca((size_t)ssprintf_needed_length_); \
    if (ssprintf_buffer_ != nullptr)  { \
        if (snprintf(ssprintf_buffer_, (size_t)ssprintf_needed_length_, \
                fmt __VA_OPT__(,) __VA_ARGS__) != ssprintf_needed_length_ - 1) \
            ssprintf_buffer_ = nullptr; \
    } \
    ssprintf_buffer_; \
})
