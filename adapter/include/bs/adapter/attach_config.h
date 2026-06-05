#ifndef BS_ADAPTER_ATTACH_CONFIG_H
#define BS_ADAPTER_ATTACH_CONFIG_H

/*
 * C-ST-7 contract block:
 * Thread safety: Read/write APIs use attach_session guards (XX-CONC); one writer per ctx.
 * Error semantics: 0 ok; -1 invalid; propagates ConfigManager load/reload/hot_update codes.
 * Platform notes: Replaces ad-hoc EventBus usage on attach freeze and reload success paths.
 */

#include "bs/kernel/state/ConfigState.h"
#include "bs/kernel/state/EventBus.h"

#include "bs/adapter/attach_context.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /** Registry freeze marker path (IMPL-08-09 / R8-07). */
#define BS_ADAPTER_CONFIG_PATH_ATTACH_FROZEN "/config/attach/frozen"

    /** EventBus facet of ctx's ConfigManager (NULL if no manager). */
    EventBus* bs_adapter_attach_config_event_bus(AttachContext* ctx);

    int bs_adapter_attach_config_get_state(AttachContext* ctx, const char* config_path,
                                           ConfigState* state);

    int bs_adapter_attach_config_get_snapshot(AttachContext* ctx, const char* config_path,
                                              void** data, size_t* size);

    /** Returns 1 if ctx has a ConfigManager (reload CM sync enabled). */
    int bs_adapter_attach_config_has_manager(AttachContext* ctx);

    /**
     * Route bytes to load_config / reload_config / hot_update from current ConfigState (B-01/B-02).
     * INITIAL|CLOSED|not-found -> load; ACTIVE -> hot_update; LOADING|UPDATING|ERROR -> reload.
     */
    int bs_adapter_attach_config_sync_path(AttachContext* ctx, const char* config_path,
                                           const void* data, size_t data_size);

    /** After registry freeze: publish attach-frozen config into ConfigManager. */
    int bs_adapter_attach_notify_registry_frozen(AttachContext* ctx);

    /** Snapshot ConfigManager path state before PER_BATCH tentative sync (XVII-CM-3). */
    typedef struct BsAttachConfigPathCheckpoint
    {
        int         had_prior;
        ConfigState prior_state;
        void*       prior_data;
        size_t      prior_size;
    } BsAttachConfigPathCheckpoint;

    int bs_adapter_attach_config_checkpoint_path(AttachContext* ctx, const char* config_path,
                                                 BsAttachConfigPathCheckpoint* out);

    void bs_adapter_attach_config_checkpoint_release(BsAttachConfigPathCheckpoint* checkpoint);

    /** Restore ConfigManager to checkpoint taken before tentative sync. */
    int bs_adapter_attach_config_rollback_path(AttachContext* ctx, const char* config_path,
                                               const BsAttachConfigPathCheckpoint* checkpoint);

#if defined(BS_TESTING)
    /** Force bs_adapter_attach_config_sync_path to fail for config_path (T6 / XVII-ORCH-1 tests).
     */
    void bs_adapter_attach_config_testing_set_sync_fail_path(const char* config_path);
    void bs_adapter_attach_config_testing_clear_sync_fail_path(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_ATTACH_CONFIG_H */
