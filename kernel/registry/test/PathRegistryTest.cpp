#include "bs/kernel/registry/path_registry.h"

#include <cassert>
#include <cstring>

int main()
{
    PathRegistry* reg = bs_path_registry_create();
    assert(reg != nullptr);

    assert(bs_path_registry_advance_phase(reg, BS_REGISTRY_PHASE_P0) ==
           BS_REGISTRY_ERR_INVALID_ARG);
    assert(bs_path_registry_advance_phase(reg, BS_REGISTRY_PHASE_FROZEN) ==
           BS_REGISTRY_ERR_INVALID_ARG);
    assert(bs_path_registry_advance_phase(reg, BS_REGISTRY_PHASE_P2) == BS_REGISTRY_ERR_PHASE);

    assert(bs_path_registry_advance_phase(reg, BS_REGISTRY_PHASE_P1) == BS_REGISTRY_OK);
    assert(bs_path_registry_advance_phase(reg, BS_REGISTRY_PHASE_P1) == BS_REGISTRY_ERR_PHASE);

    PathEntry builtin{};
    builtin.source          = BS_PATH_ENTRY_BUILTIN;
    builtin.manifest_ref    = "builtin";
    builtin.type_constraint = nullptr;

    assert(bs_path_registry_register_declaration(reg, "/kernel/ir/resolver/default", &builtin) ==
           BS_REGISTRY_OK);

    int stub = 42;
    assert(bs_path_registry_bind_instance(reg, "/kernel/ir/resolver/default", &stub) ==
           BS_REGISTRY_OK);

    Binding out{};
    assert(bs_path_registry_resolve(reg, "/kernel/ir/resolver/default", &out) == BS_REGISTRY_OK);
    assert(out.impl == &stub);

    assert(bs_path_registry_unregister(reg, "/kernel/ir/resolver/default") == BS_REGISTRY_OK);
    assert(bs_path_registry_resolve(reg, "/kernel/ir/resolver/default", &out) ==
           BS_REGISTRY_ERR_NOT_FOUND);

    assert(bs_path_registry_register_declaration(reg, "/evil/plugin", &builtin) ==
           BS_REGISTRY_ERR_INVALID_PATH);

    PathEntry plugin{};
    plugin.source          = BS_PATH_ENTRY_PLUGIN;
    plugin.manifest_ref    = "manifest-1";
    plugin.type_constraint = nullptr;
    assert(bs_path_registry_advance_phase(reg, BS_REGISTRY_PHASE_P2) == BS_REGISTRY_OK);
    assert(bs_path_registry_register_declaration(reg, "/adapter/plugin/test", &plugin) ==
           BS_REGISTRY_OK);

    assert(bs_path_registry_freeze(reg) == BS_REGISTRY_OK);
    assert(bs_path_registry_is_frozen(reg) == 1);
    assert(bs_path_registry_register_declaration(reg, "/adapter/plugin/late", &plugin) ==
           BS_REGISTRY_ERR_FROZEN);

    assert(bs_path_registry_advance_phase(reg, BS_REGISTRY_PHASE_P1) == BS_REGISTRY_ERR_FROZEN);

    bs_path_registry_destroy(reg);
    return 0;
}
