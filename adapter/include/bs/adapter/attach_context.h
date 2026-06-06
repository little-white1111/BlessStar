#ifndef BS_ADAPTER_ATTACH_CONTEXT_H
#define BS_ADAPTER_ATTACH_CONTEXT_H

/*
 * C-ST-7 contract block:
 * Thread safety: AttachContext is owned by reload/attach driver thread unless locked.
 * Error semantics: 0 success; -1 invalid/null; attach_config propagates CM codes.
 * Platform notes: One session owns RegistryFacade, ConfigManager, Kernel (default pipeline),
 *   and log state. Production reload uses create -> set_active -> bootstrap -> freeze_ctx
 *   (Kernel RUNNING) before bs_adapter_attach_reload_batch_run.
 * API prefixes (C-ST-14): bs_adapter_attach_ctx_* session; bs_adapter_attach_config_* /
 *   exec_* / reload_* / persist_* - see docs/BLESSSTAR_NAMING_CONTRACT.md.
 */

#include "bs/kernel/common/bs_log.h"
#include "bs/kernel/registry/registry_facade.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct ConfigManager ConfigManager;

    /** One attach session: registry + ConfigManager + Kernel (R8-02 / XVII-ATTACH-1). */
    typedef struct AttachContext AttachContext;

    AttachContext* bs_adapter_attach_ctx_create(void);
    void           bs_adapter_attach_ctx_destroy(AttachContext* ctx);

    RegistryFacade* bs_adapter_attach_ctx_registry(AttachContext* ctx);

    ConfigManager* bs_adapter_attach_ctx_config_manager(AttachContext* ctx);

    BsLogState* bs_adapter_attach_ctx_log_state(AttachContext* ctx);

    int  bs_adapter_attach_ctx_is_log_bus_bound(const AttachContext* ctx);
    void bs_adapter_attach_ctx_set_log_bus_bound(AttachContext* ctx, int bound);

    BsLogLevel bs_adapter_attach_ctx_get_log_level(const AttachContext* ctx);
    void       bs_adapter_attach_ctx_set_log_level(AttachContext* ctx, BsLogLevel level);

    /** Single active ctx for reload / attach_runtime / bootstrap delegation. */
    void           bs_adapter_attach_ctx_set_active(AttachContext* ctx);
    AttachContext* bs_adapter_attach_ctx_get_active(void);

    /** T20.0b: bracket get_active; debug builds assert when depth is zero. */
    void bs_adapter_attach_ctx_active_access_enter(void);
    void bs_adapter_attach_ctx_active_access_leave(void);

    /** Bind external RegistryFacade; ctx does not take ownership. */
    void bs_adapter_attach_ctx_use_external_registry(AttachContext* ctx, RegistryFacade* facade);

    /** 1 if ctx has a Kernel in KERNEL_STATE_RUNNING (after freeze_ctx). */
    int bs_adapter_attach_ctx_is_kernel_running(const AttachContext* ctx);

    /** 1 if KernelPool warmup completed on ctx (day21 A''' reload exec gate). */
    int bs_adapter_attach_ctx_is_kernel_pool_warmed(const AttachContext* ctx);

    void bs_adapter_attach_ensure_active_ctx(void);

    /** Legacy shell for log shutdown only; does not own registry or ConfigManager (XVII-ATTACH). */
    AttachContext* bs_adapter_attach_ctx_legacy_bootstrap(void);

    /** Shutdown log buses on legacy/ephemeral/active ctx (CI LSan; idempotent). */
    void bs_adapter_attach_ctx_shutdown_all_logs(void);

#ifdef __cplusplus
}

/** RAII for T20.0b active-ctx access guard. */
struct AttachActiveGuard
{
    AttachActiveGuard()
    {
        bs_adapter_attach_ctx_active_access_enter();
    }
    AttachActiveGuard(const AttachActiveGuard&)            = delete;
    AttachActiveGuard& operator=(const AttachActiveGuard&) = delete;
    ~AttachActiveGuard()
    {
        bs_adapter_attach_ctx_active_access_leave();
    }
};

#endif

#endif /* BS_ADAPTER_ATTACH_CONTEXT_H */
