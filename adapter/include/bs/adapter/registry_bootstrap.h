#ifndef BS_ADAPTER_REGISTRY_BOOTSTRAP_H
#define BS_ADAPTER_REGISTRY_BOOTSTRAP_H

/*
 * C-ST-7 contract block:
 * Thread safety: Bootstrap runs once per attach session on the driver thread.
 * Error semantics: 0 success; -1 invalid ctx/phase/plugin/freeze/notify/kernel start failure.
 * Platform notes: Active AttachContext owns RegistryFacade, ConfigManager, Kernel, and default
 *   pipeline. P1 registers builtin gate, log bus, and /kernel/pipeline/default. freeze_ctx loads
 *   plugins, freezes registry, calls bs_adapter_attach_notify_registry_frozen (ConfigManager sync),
 *   then bs_kernel_start (XVII-ATTACH-5 / XVII-KERNEL-3).
 * Integration: Facade-parameter APIs (bootstrap_begin/register_standard_io/freeze) require an
 *   active AttachContext whose registry matches `facade`; otherwise they return -1. Prefer
 *   *_ctx(AttachContext*) on the attach driver thread to avoid facade/active mismatch.
 */

#include "bs/kernel/registry/registry_facade.h"

#include "bs/adapter/attach_context.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Begin bootstrap on the active AttachContext whose registry is `facade` (XVII-ATTACH-2).
     * Returns -1 if there is no matching active ctx.
     */
    int bs_adapter_registry_bootstrap_begin(RegistryFacade* facade);

    /**
     * Load io-standard only (facade API; requires matching active ctx).
     * Prefer bs_adapter_registry_bootstrap_freeze_ctx which calls plugin_loader_load_all.
     */
    int bs_adapter_registry_bootstrap_register_standard_io(RegistryFacade* facade);

    /**
     * Freeze via active ctx matching `facade` (delegates to `bootstrap_freeze_ctx`).
     * freeze_ctx: plugin load -> registry freeze -> attach_notify_registry_frozen -> kernel start.
     * Returns -1 when active is null, registry mismatch, or any freeze step fails (XVII-ATTACH-2).
     */
    int bs_adapter_registry_bootstrap_freeze(RegistryFacade* facade);

    /** Bootstrap via AttachContext (sets active ctx; registers pipeline hub in P1). */
    int bs_adapter_registry_bootstrap_begin_ctx(AttachContext* ctx);
    int bs_adapter_registry_bootstrap_register_standard_io_ctx(AttachContext* ctx);
    /**
     * Freeze registry on ctx; notify ConfigManager (attach-frozen path); start Kernel runtime.
     * Returns -1 if log bus not bound, plugin load/freeze/notify/kernel start fails.
     */
    int bs_adapter_registry_bootstrap_freeze_ctx(AttachContext* ctx);

    /** Release spdlog / log bus (call before exit or after facade-only bootstrap tests). */
    void bs_adapter_registry_shutdown_log(void);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_REGISTRY_BOOTSTRAP_H */
