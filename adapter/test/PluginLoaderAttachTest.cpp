/**
 * IMPL-08-17: P2 plugins loaded via attach_plugins manifest (built-in table) before freeze.
 */

#include "bs/kernel/registry/registry_facade.h"

#include "bs/adapter/attach_context.h"
#include "bs/adapter/registry_bootstrap.h"

#include <cassert>

int main()
{
    AttachContext* ctx = bs_adapter_attach_ctx_create();
    assert(ctx != nullptr);

    assert(bs_adapter_registry_bootstrap_begin_ctx(ctx) == 0);
    assert(bs_registry_facade_current_phase(bs_adapter_attach_ctx_registry(ctx)) ==
           BS_REGISTRY_PHASE_P1);

    assert(bs_adapter_registry_bootstrap_freeze_ctx(ctx) == 0);
    assert(bs_registry_facade_current_phase(bs_adapter_attach_ctx_registry(ctx)) ==
           BS_REGISTRY_PHASE_FROZEN);

    Binding io_local{};
    assert(bs_registry_facade_resolve(bs_adapter_attach_ctx_registry(ctx), "/adapter/io/local",
                                      &io_local) == BS_REGISTRY_OK);
    assert(io_local.impl != nullptr);

    Binding io_db{};
    assert(bs_registry_facade_resolve(bs_adapter_attach_ctx_registry(ctx), "adapter.io.db",
                                      &io_db) == BS_REGISTRY_OK);

    Binding reload_batch{};
    assert(bs_registry_facade_resolve(bs_adapter_attach_ctx_registry(ctx),
                                      "/adapter/orchestration/reload_batch",
                                      &reload_batch) == BS_REGISTRY_OK);
    assert(reload_batch.impl != nullptr);

    bs_adapter_attach_ctx_destroy(ctx);
    bs_adapter_registry_shutdown_log();
    return 0;
}
