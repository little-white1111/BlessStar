#ifndef BS_ADAPTER_ATTACH_CONTEXT_H
#define BS_ADAPTER_ATTACH_CONTEXT_H

#include "bs/kernel/common/bs_log.h"
#include "bs/kernel/registry/registry_facade.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * One attach / one bootstrap chain runtime handle (R8-02 · ATTACH-VIII).
     * MVP: single process may hold one active ctx for attach_runtime forwarding (phase 1).
     */
    typedef struct AttachContext AttachContext;

    AttachContext* bs_attach_context_create(void);
    void           bs_attach_context_destroy(AttachContext* ctx);

    RegistryFacade* bs_attach_context_registry(AttachContext* ctx);

    BsLogState* bs_attach_context_log_state(AttachContext* ctx);

    int  bs_attach_context_is_log_bus_bound(const AttachContext* ctx);
    void bs_attach_context_set_log_bus_bound(AttachContext* ctx, int bound);

    BsLogLevel bs_attach_context_get_log_level(const AttachContext* ctx);
    void       bs_attach_context_set_log_level(AttachContext* ctx, BsLogLevel level);

    /** Phase 1: optional active ctx for attach_runtime / legacy bootstrap compat. */
    void           bs_attach_context_set_active(AttachContext* ctx);
    AttachContext* bs_attach_context_get_active(void);

    /** Legacy bootstrap bridge: ctx does not own registry (IMPL-08-06 phase 2). */
    void bs_attach_context_use_external_registry(AttachContext* ctx, RegistryFacade* facade);

    void bs_adapter_attach_ensure_active_ctx(void);

    /** Process-wide legacy ctx for `bootstrap_begin(facade)` without caller-owned ctx. */
    AttachContext* bs_attach_context_legacy_bootstrap(void);

    /** Shutdown log buses on legacy/ephemeral/active ctx (CI LSan; idempotent). */
    void bs_attach_context_shutdown_all_logs(void);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_ATTACH_CONTEXT_H */
