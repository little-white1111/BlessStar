#ifndef BS_ADAPTER_PERSISTENCE_ATTACH_STORE_H
#define BS_ADAPTER_PERSISTENCE_ATTACH_STORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum BsAttachScheme
    {
        BS_ATTACH_SCHEME_UNSET     = -1,
        BS_ATTACH_SCHEME_PER_PATH  = 0,
        BS_ATTACH_SCHEME_PER_BATCH = 1
    } BsAttachScheme;

    typedef enum BsAttachStatus
    {
        BS_ATTACH_OK              = 0,
        BS_ATTACH_ERR_INVALID_ARG = -1,
        BS_ATTACH_ERR_CONFLICT    = -2,
        BS_ATTACH_ERR_OOM         = -3,
        BS_ATTACH_ERR_IO          = -4,
        BS_ATTACH_ERR_LIMIT       = -5
    } BsAttachStatus;

    typedef struct BsAttachStore BsAttachStore;

    /** Max manifest text line length when loading from disk (AUD-IX-12). */
#define BS_ATTACH_MAX_MANIFEST_LINE (8192u)

    /** File-backed manifest at @p manifest_path. Pass NULL for in-memory test store. */
    BsAttachStore* bs_attach_store_open(const char* manifest_path);
    void           bs_attach_store_close(BsAttachStore* store);

    uint64_t bs_attach_store_batch_epoch(const BsAttachStore* store);

    /** @p expected_rev is revision at session start; 0 for first write. */
    int bs_attach_store_get_revision(const BsAttachStore* store, const char* uri,
                                     uint64_t* rev_out);

    /** Canonical path from manifest (RES-IX-8 uri->path); empty if unknown. */
    int bs_attach_store_get_canonical_path(const BsAttachStore* store, const char* uri,
                                           char* out_path, size_t out_cap);

    int bs_attach_store_commit_per_path(BsAttachStore* store, const char* uri, const void* data,
                                        size_t len, uint64_t expected_rev);

    void bs_attach_store_batch_begin(BsAttachStore* store);
    int  bs_attach_store_batch_stage(BsAttachStore* store, const char* uri, const void* data,
                                     size_t len, uint64_t expected_rev);
    /** Atomic manifest + all staged canonical files (RES-IX-10). */
    int  bs_attach_store_batch_commit(BsAttachStore* store);
    void bs_attach_store_batch_abort(BsAttachStore* store);

    typedef void* (*BsAttachMallocFn)(size_t size);

#if defined(BS_TESTING)
    /** Testing only: inject malloc failures (see ZK LibCMocks pattern). */
    void bs_attach_store_set_malloc_hook(BsAttachMallocFn fn);
    void bs_attach_store_reset_malloc_hook(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PERSISTENCE_ATTACH_STORE_H */
