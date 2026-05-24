#ifndef BS_ADAPTER_REGISTRY_BOOTSTRAP_H
#define BS_ADAPTER_REGISTRY_BOOTSTRAP_H

#include "bs/kernel/registry/registry_facade.h"

#include "bs/adapter/attach_context.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Attach-phase registry bootstrap (R-II-2):
     * 1) validate builtin requirements
     * 2) register built-in /kernel extension declarations + hub mappings (phase P1)
     * Caller may register other /adapter plugins in P2 before freeze.
     * Does NOT freeze; ends in phase P1.
     */
    int bs_adapter_registry_bootstrap_begin(RegistryFacade* facade);

    /**
     * Load io-standard plugin only (legacy API; IMPL-08-17).
     * Prefer freeze path which calls bs_adapter_plugin_loader_load_all.
     */
    int bs_adapter_registry_bootstrap_register_standard_io(RegistryFacade* facade);

    /**
     * Load all enabled P2 plugins (attach_plugins.yaml / built-in table), then freeze (R-II-2).
     */
    int bs_adapter_registry_bootstrap_freeze(RegistryFacade* facade);

    /** R8-02 · IMPL-08-06 phase 1: bootstrap via AttachContext (sets active ctx). */
    int bs_adapter_registry_bootstrap_begin_ctx(AttachContext* ctx);
    int bs_adapter_registry_bootstrap_register_standard_io_ctx(AttachContext* ctx);
    int bs_adapter_registry_bootstrap_freeze_ctx(AttachContext* ctx);

    /** R8-07 · IMPL-08-10: optional hook after successful freeze (no ConfigManager in bootstrap).
     */
    typedef void (*BsAdapterStateNotifierFn)(RegistryFacade* facade, void* user_data);

    void bs_adapter_registry_register_state_notifier(BsAdapterStateNotifierFn fn, void* user_data);
    void bs_adapter_registry_clear_state_notifier(void);

    /** Release spdlog / log bus (call before exit or after facade-only bootstrap tests). */
    void bs_adapter_registry_shutdown_log(void);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_REGISTRY_BOOTSTRAP_H */
