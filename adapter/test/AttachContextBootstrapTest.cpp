#include "bs/kernel/registry/registry_facade.h"

#include "bs/adapter/attach_context.h"
#include "bs/adapter/registry_bootstrap.h"

#include <cassert>

int main()
{
    AttachContext* ctx = bs_attach_context_create();
    assert(ctx != nullptr);
    assert(bs_attach_context_registry(ctx) != nullptr);

    assert(bs_adapter_registry_bootstrap_begin_ctx(ctx) == 0);
    assert(bs_attach_context_is_log_bus_bound(ctx));
    assert(bs_registry_facade_current_phase(bs_attach_context_registry(ctx)) ==
           BS_REGISTRY_PHASE_P1);

    assert(bs_adapter_registry_bootstrap_register_standard_io_ctx(ctx) == 0);
    assert(bs_adapter_registry_bootstrap_freeze_ctx(ctx) == 0);
    assert(bs_registry_facade_current_phase(bs_attach_context_registry(ctx)) ==
           BS_REGISTRY_PHASE_FROZEN);

    bs_attach_context_destroy(ctx);
    return 0;
}
