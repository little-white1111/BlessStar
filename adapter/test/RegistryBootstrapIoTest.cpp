#include "bs/kernel/registry/registry_facade.h"

#include "bs/adapter/attach_context.h"
#include "bs/adapter/registry_bootstrap.h"

#include <cassert>

int main()
{
    AttachContext* ctx = bs_adapter_attach_ctx_create();
    assert(ctx != nullptr);
    bs_adapter_attach_ctx_set_active(ctx);
    RegistryFacade* facade = bs_adapter_attach_ctx_registry(ctx);
    assert(facade != nullptr);

    assert(bs_adapter_registry_bootstrap_begin_ctx(ctx) == 0);
    assert(bs_registry_facade_current_phase(facade) == BS_REGISTRY_PHASE_P1);

    assert(bs_adapter_registry_bootstrap_register_standard_io_ctx(ctx) == 0);
    assert(bs_registry_facade_current_phase(facade) == BS_REGISTRY_PHASE_P2);

    Binding local{};
    assert(bs_registry_facade_resolve(facade, "/adapter/io/local", &local) == BS_REGISTRY_OK);
    assert(local.impl != nullptr);

    assert(bs_adapter_registry_bootstrap_register_standard_io(facade) == 0);

    bs_adapter_registry_shutdown_log();
    bs_adapter_attach_ctx_destroy(ctx);
    return 0;
}
