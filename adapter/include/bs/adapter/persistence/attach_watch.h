#ifndef BS_ADAPTER_PERSISTENCE_ATTACH_WATCH_H
#define BS_ADAPTER_PERSISTENCE_ATTACH_WATCH_H

/*
 * C-ST-7 contract block:
 * Thread safety: Watch registry guarded by internal mutex in attach_watch.c.
 * Error semantics: BS_ATTACH_* on register failure; callbacks on watcher thread.
 * Platform notes: File watch integration for config hot reload (platform-specific).
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum BsAttachWatchStage
    {
        BS_ATTACH_WATCH_STAGE_CAS = 1,
        BS_ATTACH_WATCH_STAGE_WAL_FSYNC,
        BS_ATTACH_WATCH_STAGE_CANONICAL_WRITE,
        BS_ATTACH_WATCH_STAGE_MANIFEST_FLIP,
        BS_ATTACH_WATCH_STAGE_WAL_COMMIT,
        BS_ATTACH_WATCH_STAGE_RECOVER_CONSERVATIVE
    } BsAttachWatchStage;

    typedef enum BsAttachWatchResult
    {
        BS_ATTACH_WATCH_RESULT_OK   = 0,
        BS_ATTACH_WATCH_RESULT_FAIL = 1
    } BsAttachWatchResult;

    typedef struct BsAttachWatchEvent
    {
        uint64_t            epoch;
        const char*         uri;
        BsAttachWatchStage  stage;
        BsAttachWatchResult result;
    } BsAttachWatchEvent;

    typedef int (*BsAttachWatchSubscriber)(const BsAttachWatchEvent* ev, void* user);

    int  bs_adapter_attach_persist_watch_subscribe(BsAttachWatchSubscriber fn, void* user,
                                                   int* token_out);
    void bs_adapter_attach_persist_watch_unsubscribe(int token);
    int  bs_adapter_attach_persist_watch_publish(const BsAttachWatchEvent* ev);

    typedef struct BsAttachWatchMetrics
    {
        uint64_t total_events;
        uint64_t stage_counts[8];
        uint64_t fail_count;
        uint64_t dropped_duplicates;
        uint64_t dedupe_capacity;
    } BsAttachWatchMetrics;

    void bs_adapter_attach_persist_watch_metrics_reset(void);
    int  bs_adapter_attach_persist_watch_metrics_on_event(const BsAttachWatchEvent* ev, void* user);
    void bs_adapter_attach_persist_watch_metrics_snapshot(BsAttachWatchMetrics* out);

    typedef struct BsAttachWatchAudit
    {
        uint64_t conservative_recover_count;
        uint64_t publish_fail_count;
        uint64_t publish_callback_error_count;
        uint64_t callback_error_by_stage[8];
        uint64_t last_callback_error_epoch;
        uint32_t last_callback_error_stage;
    } BsAttachWatchAudit;

    void bs_adapter_attach_persist_watch_audit_reset(void);
    int  bs_adapter_attach_persist_watch_audit_on_event(const BsAttachWatchEvent* ev, void* user);
    void bs_adapter_attach_persist_watch_audit_snapshot(BsAttachWatchAudit* out);

    size_t bs_adapter_attach_persist_watch_dedupe_capacity(void);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PERSISTENCE_ATTACH_WATCH_H */
