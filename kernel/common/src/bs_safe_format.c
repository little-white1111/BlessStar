#include "bs/kernel/common/bs_safe_format.h"

#include <stdio.h>

int bs_safe_vsnprintf(char* buf, size_t cap, const char* fmt, va_list ap)
{
    if (!buf || cap == 0)
        return 0;
    const int n  = vsnprintf(buf, cap, fmt, ap);
    buf[cap - 1] = '\0';
    return n;
}

int bs_safe_snprintf(char* buf, size_t cap, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const int n = bs_safe_vsnprintf(buf, cap, fmt, ap);
    va_end(ap);
    return n;
}
