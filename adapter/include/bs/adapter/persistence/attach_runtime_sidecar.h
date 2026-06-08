#ifndef BS_ADAPTER_PERSISTENCE_ATTACH_RUNTIME_SIDECAR_H
#define BS_ADAPTER_PERSISTENCE_ATTACH_RUNTIME_SIDECAR_H

/*
 * C-ST-7 contract block:
 * Thread safety: sidecar file IO is process-local; caller serializes with manifest writes.
 * Error semantics: validate returns 0/1; write/invalidate return BS_ATTACH_* codes.
 * Platform notes: runtime.ckpt is advisory only; manifest+WAL remain authoritative.
 */

#include "bs/adapter/persistence/attach_store.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define BS_ATTACH_SIDECAR_MAGIC   0x4B435452u /* 'RTCK' */
#define BS_ATTACH_SIDECAR_VERSION 1u

#define BS_ATTACH_SIDECAR_FLAG_CLEAN_SHUTDOWN 0x0001u

    typedef struct BsAttachRuntimeSidecarEntry
    {
        const char* uri;
        uint64_t    revision;
        uint32_t    payload_digest;
    } BsAttachRuntimeSidecarEntry;

    int bs_adapter_attach_persist_sidecar_path_for_manifest(const char* manifest_path, char* out,
                                                            size_t out_cap);

    int bs_adapter_attach_persist_sidecar_invalidate(const char* manifest_path);

    int bs_adapter_attach_persist_sidecar_write(const char*          manifest_path,
                                                const BsAttachStore* store, uint32_t flags);

    /** Returns 1 if sidecar matches store manifest epoch/digest/revisions; 0 if absent/mismatch. */
    int bs_adapter_attach_persist_sidecar_validate(const char*          manifest_path,
                                                   const BsAttachStore* store,
                                                   uint32_t             required_flags);

    int bs_adapter_attach_persist_manifest_file_digest(const char* manifest_path,
                                                       uint32_t*   digest_out);

    int bs_adapter_attach_persist_read_file_digest(const char* path, uint32_t* digest_out);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PERSISTENCE_ATTACH_RUNTIME_SIDECAR_H */
