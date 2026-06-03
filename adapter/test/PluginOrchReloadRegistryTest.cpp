/* IMPL-08-17 phase 3: orch-reload registers reload_batch factory in PathRegistry. */

#include "bs/kernel/registry/registry_facade.h"

#include "bs/adapter/attach_context.h"
#include "bs/adapter/orchestration/reload_batch_factory.h"
#include "bs/adapter/orchestration/reload_gate_default.h"
#include "bs/adapter/persistence/attach_store.h"
#include "bs/adapter/registry_bootstrap.h"

#include <cassert>

int main()
{
    AttachContext* ctx = bs_adapter_attach_ctx_create();
    assert(ctx != nullptr);

    assert(bs_adapter_registry_bootstrap_begin_ctx(ctx) == 0);
    assert(bs_adapter_registry_bootstrap_freeze_ctx(ctx) == 0);

    RegistryFacade* facade = bs_adapter_attach_ctx_registry(ctx);

    Binding batch{};
    assert(bs_registry_facade_resolve(facade, "/adapter/orchestration/reload_batch", &batch) ==
           BS_REGISTRY_OK);
    assert(batch.impl != nullptr);

    ReloadBatchController* ctrl = bs_adapter_attach_reload_batch_create_from_binding(&batch, 4);
    assert(ctrl != nullptr);
    bs_adapter_attach_reload_batch_set_default_gate(ctrl);
    bs_adapter_attach_reload_batch_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_PATH);
    assert(bs_adapter_attach_reload_batch_add_path(ctrl, "file:///orch-registry-smoke") == 0);
    assert(bs_adapter_attach_reload_batch_run(ctrl) != 0); /* no read_fn wired; factory create ok */

    bs_adapter_attach_reload_batch_destroy_from_binding(&batch, ctrl);
    bs_adapter_attach_ctx_destroy(ctx);
    bs_adapter_registry_shutdown_log();
    return 0;
}
