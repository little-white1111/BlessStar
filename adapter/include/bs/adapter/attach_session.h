#ifndef BS_ADAPTER_ATTACH_SESSION_H
#define BS_ADAPTER_ATTACH_SESSION_H

/*
 * C-ST-7 contract block:
 * Thread safety: AttachSession guards serialize readers vs writers on one AttachContext (XX-CONC).
 * Error semantics: see attach_errors.h; 0 ok.
 * Platform notes: Write window used by reload_batch_run; read guards for snapshot/meta/chunk APIs.
 */

#include "bs/adapter/attach_context.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct BsAttachSnapshotMeta
    {
        uint64_t revision;
        size_t   total_size;
        uint32_t chunk_cap;
    } BsAttachSnapshotMeta;

    void bs_adapter_attach_session_init(AttachContext* ctx);
    void bs_adapter_attach_session_destroy(AttachContext* ctx);

    /** Block new readers; exclusive write until end_write_window. */
    void bs_adapter_attach_session_begin_write_window(AttachContext* ctx);
    void bs_adapter_attach_session_end_write_window(AttachContext* ctx);

    /** Block until phase-2 watch notify jobs finish (call before end_write_window). */
    void bs_adapter_attach_session_drain_pending_notifications(AttachContext* ctx);

    int  bs_adapter_attach_session_try_read_lock(AttachContext* ctx);
    void bs_adapter_attach_session_read_unlock(AttachContext* ctx);

    int  bs_adapter_attach_session_try_write_lock(AttachContext* ctx);
    void bs_adapter_attach_session_write_unlock(AttachContext* ctx);

    uint64_t bs_adapter_attach_session_path_revision(AttachContext* ctx, const char* path);
    void     bs_adapter_attach_session_bump_revision(AttachContext* ctx, const char* path);

    /** Non-zero when reload write window holds the session exclusive lock. */
    int bs_adapter_attach_session_in_write_window(AttachContext* ctx);

    /** REC-A'-4: explicit crash recovery blocks read APIs until cold reload completes. */
    void bs_adapter_attach_session_set_recovering(AttachContext* ctx, int recovering);
    int  bs_adapter_attach_session_is_recovering(AttachContext* ctx);

    int bs_adapter_attach_config_get_snapshot_meta(AttachContext* ctx, const char* config_path,
                                                   BsAttachSnapshotMeta* out);

    /**
     * Copy full snapshot when total_size <= max_bytes; else BS_ATTACH_CONC_ERR_TOO_LARGE.
     */
    int bs_adapter_attach_config_get_snapshot_copy(AttachContext* ctx, const char* config_path,
                                                   void* buf, size_t buf_cap, size_t* out_size,
                                                   uint64_t* revision_out);

    int bs_adapter_attach_config_open_snapshot_read(AttachContext* ctx, const char* config_path,
                                                    int* handle_out, uint64_t* revision_out);

    int bs_adapter_attach_config_read_snapshot_chunk(AttachContext* ctx, int handle, size_t offset,
                                                     void* buf, size_t buf_cap, size_t* out_len);

    void bs_adapter_attach_config_close_snapshot_read(AttachContext* ctx, int handle);

    /** Strong-consistency optional (XX-CONC-1): wait until path revision >= revision_min. */
    int bs_adapter_attach_config_wait_notify(AttachContext* ctx, const char* config_path,
                                             uint64_t revision_min, int timeout_ms);

    int bs_adapter_attach_config_read_since_meta(AttachContext* ctx, const char* config_path,
                                                 uint64_t revision_min, int timeout_ms,
                                                 BsAttachSnapshotMeta* out);

#ifdef __cplusplus
}

struct AttachReadGuard
{
    AttachContext* ctx;
    int            rc;

    explicit AttachReadGuard(AttachContext* c)
        : ctx(c)
        , rc(bs_adapter_attach_session_try_read_lock(c))
    {
    }
    AttachReadGuard(const AttachReadGuard&)            = delete;
    AttachReadGuard& operator=(const AttachReadGuard&) = delete;
    ~AttachReadGuard()
    {
        if (rc == 0)
            bs_adapter_attach_session_read_unlock(ctx);
    }
    int status() const
    {
        return rc;
    }
};

struct AttachWriteGuard
{
    AttachContext* ctx;
    int            rc;

    explicit AttachWriteGuard(AttachContext* c)
        : ctx(c)
        , rc(bs_adapter_attach_session_try_write_lock(c))
    {
    }
    AttachWriteGuard(const AttachWriteGuard&)            = delete;
    AttachWriteGuard& operator=(const AttachWriteGuard&) = delete;
    ~AttachWriteGuard()
    {
        if (rc == 0)
            bs_adapter_attach_session_write_unlock(ctx);
    }
    int status() const
    {
        return rc;
    }
};

#endif

#endif /* BS_ADAPTER_ATTACH_SESSION_H */
