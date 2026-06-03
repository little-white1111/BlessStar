#ifndef BS_KERNEL_COMMON_BS_SAFE_FORMAT_H
#define BS_KERNEL_COMMON_BS_SAFE_FORMAT_H

/*
 * C-ST-7 contract block:
 * Thread safety: bs_safe_snprintf is reentrant when output buffer is caller-owned.
 * Error semantics: Returns required length excluding NUL; truncates safely.
 * Platform notes: Used instead of raw snprintf in C ABI paths.
 */

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
