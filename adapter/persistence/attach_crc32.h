#ifndef BS_ADAPTER_PERSISTENCE_ATTACH_CRC32_H
#define BS_ADAPTER_PERSISTENCE_ATTACH_CRC32_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    uint32_t bs_attach_crc32(const void* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PERSISTENCE_ATTACH_CRC32_H */
