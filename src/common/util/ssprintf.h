#include <stdarg.h>
#include <stdio.h>

static inline const char* ssprintf(const char* fmt, ...)
{
    static char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    return buffer;
}
