#ifndef BS_KERNEL_SCHEMA_COMPAT_H
#define BS_KERNEL_SCHEMA_COMPAT_H

/*
 * MSVC/POSIX portability helpers for schema library.
 * Provides strdup, strndup, etc. as needed.
 */

#ifdef _MSC_VER
#include <malloc.h>
#else
#include <alloca.h>
#endif

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline char* bs_strdup(const char* s)
{
    if (!s) return NULL;
    size_t len = strlen(s);
    char*  d   = (char*)malloc(len + 1);
    if (d) { memcpy(d, s, len + 1); }
    return d;
}

static inline char* bs_strndup(const char* s, size_t n)
{
    if (!s) return NULL;
    size_t slen = strlen(s);
    if (slen > n) slen = n;
    char* d = (char*)malloc(slen + 1);
    if (d) { memcpy(d, s, slen); d[slen] = '\0'; }
    return d;
}

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_SCHEMA_COMPAT_H */
