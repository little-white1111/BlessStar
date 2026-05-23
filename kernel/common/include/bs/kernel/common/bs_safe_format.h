#ifndef BS_KERNEL_COMMON_BS_SAFE_FORMAT_H
#define BS_KERNEL_COMMON_BS_SAFE_FORMAT_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Always writes a NUL terminator when @p cap > 0.
     * Returns the number of characters that would have been written (excluding NUL),
     * or a negative value on encoding error (same contract as snprintf).
     */
    int bs_safe_snprintf(char* buf, size_t cap, const char* fmt, ...);

    int bs_safe_vsnprintf(char* buf, size_t cap, const char* fmt, va_list ap);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_COMMON_BS_SAFE_FORMAT_H */
