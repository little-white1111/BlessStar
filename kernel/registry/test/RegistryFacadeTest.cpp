#include "bs/kernel/registry/registry_facade.h"

#include <cassert>
#include <cstring>

int main()
{
    RegistryFacade* facade = bs_registry_facade_create();
    assert(facade != nullptr);
    assert(bs_registry_facade_advance_phase(facade, BS_REGISTRY_PHASE_P1) == BS_REGISTRY_OK);
    assert(bs_registry_facade_advance_phase(facade, BS_REGISTRY_PHASE_P2) == BS_REGISTRY_OK);

    PathEntry plugin{};
    plugin.source          = BS_PATH_ENTRY_PLUGIN;
    plugin.manifest_ref    = "m-ref";
    plugin.type_constraint = nullptr;

    assert(bs_registry_facade_verify_manifest_ref("/adapter/plugin/x", nullptr) ==
           BS_REGISTRY_ERR_MANIFEST);
    assert(bs_registry_facade_verify_manifest_ref("/adapter/plugin/x", "ok") == BS_REGISTRY_OK);

    assert(bs_registry_facade_register_declaration(facade, "/adapter/plugin/vendor_x", &plugin) ==
           BS_REGISTRY_OK);
    assert(bs_registry_facade_register_hub_mapping(
               facade, "adapter.plugin.vendor_x", "/adapter/plugin/vendor_x", 0) == BS_REGISTRY_OK);

    int impl = 7;
    assert(bs_registry_facade_bind_instance(facade, "/adapter/plugin/vendor_x", &impl) ==
           BS_REGISTRY_OK);

    Binding b{};
    assert(bs_registry_facade_resolve(facade, "adapter.plugin.vendor_x", &b) == BS_REGISTRY_OK);
    assert(b.impl == &impl);

    assert(bs_registry_facade_resolve(facade, "/adapter/plugin/vendor_x", &b) == BS_REGISTRY_OK);

    assert(bs_registry_facade_snapshot_id(facade) == 0);

    bs_registry_facade_destroy(facade);
    return 0;
}
