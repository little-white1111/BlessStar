#include "bs/kernel/registry/registry_facade.h"

#include <cassert>

int main()
{
    RegistryFacade* facade = bs_registry_facade_create();
    assert(facade != nullptr);
    assert(bs_registry_facade_advance_phase(facade, BS_REGISTRY_PHASE_P1) == BS_REGISTRY_OK);

    PathEntry bad_kernel{};
    bad_kernel.source          = BS_PATH_ENTRY_PLUGIN;
    bad_kernel.manifest_ref    = "x";
    bad_kernel.type_constraint = nullptr;
    assert(bs_registry_facade_register_declaration(facade, "/kernel/ir/evil", &bad_kernel) ==
           BS_REGISTRY_ERR_MANIFEST);

    PathEntry plugin{};
    plugin.source          = BS_PATH_ENTRY_PLUGIN;
    plugin.manifest_ref    = "m";
    plugin.type_constraint = nullptr;
    assert(bs_registry_facade_register_declaration(facade, "/not/adapter/foo", &plugin) ==
           BS_REGISTRY_ERR_INVALID_PATH);

    assert(bs_registry_facade_advance_phase(facade, BS_REGISTRY_PHASE_P2) == BS_REGISTRY_OK);
    assert(bs_registry_facade_register_declaration(facade, "/adapter/plugin/test", &plugin) ==
           BS_REGISTRY_OK);
    assert(bs_registry_facade_bind_instance(facade, "/adapter/plugin/test", &plugin) ==
           BS_REGISTRY_OK);

    assert(bs_registry_facade_freeze(facade) == BS_REGISTRY_OK);

    assert(bs_registry_facade_register_declaration(facade, "/adapter/plugin/late", &plugin) ==
           BS_REGISTRY_ERR_FROZEN);
    assert(bs_registry_facade_bind_instance(facade, "/adapter/plugin/late", &plugin) ==
           BS_REGISTRY_ERR_FROZEN);
    assert(bs_registry_facade_register_hub_mapping(facade, "adapter.plugin.late",
                                                   "/adapter/plugin/late", 0) ==
           BS_REGISTRY_ERR_FROZEN);

    bs_registry_facade_destroy(facade);
    return 0;
}
