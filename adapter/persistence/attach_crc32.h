#ifndef BS_ADAPTER_PERSISTENCE_ATTACH_CRC32_H
#define BS_ADAPTER_PERSISTENCE_ATTACH_CRC32_H

/*
 * C-ST-7 contract block:
 * Thread safety: Reentrant; no mutable state.
 * Error semantics: Always returns CRC; empty input yields 0.
 * Platform notes: IEEE polynomial; internal implementation only.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    uint32_t bs_adapter_attach_persist_crc32(const void* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PERSISTENCE_ATTACH_CRC32_H */
