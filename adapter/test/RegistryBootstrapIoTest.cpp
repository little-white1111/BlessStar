#include "bs/kernel/registry/registry_facade.h"

#include "bs/adapter/registry_bootstrap.h"

#include <cassert>

int main()
{
    RegistryFacade* facade = bs_registry_facade_create();
    assert(facade != nullptr);
    assert(bs_adapter_registry_bootstrap_begin(facade) == 0);
    assert(bs_registry_facade_current_phase(facade) == BS_REGISTRY_PHASE_P1);

    assert(bs_adapter_registry_bootstrap_register_standard_io(facade) == 0);
    assert(bs_registry_facade_current_phase(facade) == BS_REGISTRY_PHASE_P2);

    Binding local{};
    assert(bs_registry_facade_resolve(facade, "/adapter/io/local", &local) == BS_REGISTRY_OK);
    assert(local.impl != nullptr);

    assert(bs_adapter_registry_bootstrap_register_standard_io(facade) == 0);

    bs_registry_facade_destroy(facade);
    return 0;
}
