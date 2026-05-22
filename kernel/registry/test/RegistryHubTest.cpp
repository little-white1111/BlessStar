#include "bs/kernel/registry/registry_hub.h"

#include <cassert>
#include <cstring>

int main()
{
    RegistryHub* hub = bs_registry_hub_create();
    assert(hub != nullptr);

    assert(bs_registry_hub_register_mapping(hub, "kernel.ir.resolver.default",
                                            "/kernel/ir/resolver/default", 0) == BS_REGISTRY_OK);

    char path[BS_REGISTRY_MAX_PATH];
    assert(bs_registry_hub_resolve(hub, "kernel.ir.resolver.default", path, sizeof(path)) ==
           BS_REGISTRY_OK);
    assert(std::strcmp(path, "/kernel/ir/resolver/default") == 0);

    assert(bs_registry_hub_register_mapping(hub, "kernel.ir.resolver.default",
                                            "/kernel/ir/resolver/other", 0) ==
           BS_REGISTRY_ERR_HUB_OVERRIDE);

    assert(bs_registry_hub_register_mapping(hub, "too.few", "/kernel/x", 0) ==
           BS_REGISTRY_ERR_LOGICAL_ID);

    assert(bs_registry_hub_register_mapping(hub, "a.b.c.d.e", "/kernel/x", 0) ==
           BS_REGISTRY_ERR_LOGICAL_ID);

    bs_registry_hub_destroy(hub);
    return 0;
}
