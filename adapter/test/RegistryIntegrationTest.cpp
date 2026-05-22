#include "bs/adapter/registry_bootstrap.h"
#include "bs/adapter/requirement_filter.h"
#include "bs/kernel/ir/requirements.h"
#include "bs/kernel/registry/registry_facade.h"

#include <cassert>
#include <cstring>

int main()
{
    const KernelBuiltinRequirements* k = kernel_get_builtin_requirements();
    assert(k != nullptr);

    RegistryFacade* facade = bs_registry_facade_create();
    assert(facade != nullptr);

    assert(bs_adapter_registry_bootstrap_begin(facade) == 0);
    assert(bs_registry_facade_advance_phase(facade, BS_REGISTRY_PHASE_P2) == BS_REGISTRY_OK);

    PathEntry plugin{};
    plugin.source          = BS_PATH_ENTRY_PLUGIN;
    plugin.manifest_ref    = "adapter-manifest";
    plugin.type_constraint = nullptr;

    assert(bs_registry_facade_register_declaration(facade, "/adapter/plugin/test_plugin",
                                                   &plugin) == BS_REGISTRY_OK);
    assert(bs_registry_facade_register_hub_mapping(facade, "adapter.plugin.test_plugin",
                                                   "/adapter/plugin/test_plugin", 0) ==
           BS_REGISTRY_OK);

    int plugin_impl = 99;
    assert(bs_registry_facade_bind_instance(facade, "/adapter/plugin/test_plugin", &plugin_impl) ==
           BS_REGISTRY_OK);

    assert(bs_adapter_registry_bootstrap_freeze(facade) == 0);

    Binding gate{};
    assert(bs_registry_facade_resolve(facade, "kernel.ir.builtin_gate", &gate) == BS_REGISTRY_OK);

    Binding plug{};
    assert(bs_registry_facade_resolve(facade, "adapter.plugin.test_plugin", &plug) ==
           BS_REGISTRY_OK);
    assert(plug.impl == &plugin_impl);

    assert(bs_registry_facade_register_declaration(facade, "/adapter/plugin/after_freeze",
                                                   &plugin) == BS_REGISTRY_ERR_FROZEN);

    IRRequirementList* active =
        bs_adapter_requirement_filter_merge_activation(nullptr);
    assert(active != nullptr);
    bs_requirement_list_free(active);

    bs_registry_facade_destroy(facade);
    return 0;
}
