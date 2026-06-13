#ifndef BS_APP_SDK_ATTACH_SNAPSHOT_UTILS_H
#define BS_APP_SDK_ATTACH_SNAPSHOT_UTILS_H

/*
 * C-ST-7 contract block:
 * Thread safety: These functions are thread-safe for independent calls.
 * Error semantics: Returns nullptr on null/empty input; caller must free with *_free().
 * Platform notes: Thin C-ABI helpers for converting watch callback snapshots to text.
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Convert a snapshot (binary blob, likely text/JSON) to a null-terminated C string.
 * Returns heap-allocated text that must be freed with bs_attach_watch_snapshot_text_free().
 * Returns nullptr if snapshot is null or size is 0.
 */
static inline char* bs_attach_watch_snapshot_as_text(const void* snapshot, size_t size)
{
    if (!snapshot || size == 0)
        return (char*)0;
    char* text = (char*)malloc(size + 1);
    if (!text)
        return (char*)0;
    memcpy(text, snapshot, size);
    text[size] = '\0';
    return text;
}

/** Free a string returned by bs_attach_watch_snapshot_as_text(). */
static inline void bs_attach_watch_snapshot_text_free(char* text)
{
    free(text);
}

#ifdef __cplusplus
}
#endif

#endif /* BS_APP_SDK_ATTACH_SNAPSHOT_UTILS_H */
