/**
 * Attach-phase integration (R-II-2, architecture doc figure 2).
 *
 * In scope: implemented links only --
 *   builtin requirements -> requirement_filter -> registry_bootstrap ->
 *   P2 plugin register -> freeze -> IR gate (verify) -> resolve + use binding.
 *
 * Out of scope: CLI auto-bootstrap, config collect/translate, pipeline/report,
 * state hot-reload via registry, snapshot switch.
 */

#include "bs/kernel/ir/ir.h"
#include "bs/kernel/ir/requirements.h"
#include "bs/kernel/registry/registry_facade.h"

#include "bs/adapter/attach_context.h"
#include "bs/adapter/registry_bootstrap.h"
#include "bs/adapter/requirement_filter.h"

#include <cassert>
#include <cstring>

static int g_plugin_dispatch(int value)
{
    return value + 1;
}

int main()
{
    /* Step 1: builtin requirements (not via PathRegistry, R-II-1) */
    const KernelBuiltinRequirements* builtin = bs_kernel_get_builtin_requirements();
    assert(builtin != nullptr);
    assert(bs_adapter_requirement_filter_validate_builtin() == 0);
    assert(bs_adapter_requirement_filter_check_adapter_version("0.4.0") == 0);

    AttachContext* ctx = bs_adapter_attach_ctx_create();
    assert(ctx != nullptr);
    bs_adapter_attach_ctx_set_active(ctx);
    RegistryFacade* facade = bs_adapter_attach_ctx_registry(ctx);
    assert(facade != nullptr);

    /* Step 2: P1 built-in /kernel extension (bootstrap ends in phase P1) */
    assert(bs_adapter_registry_bootstrap_begin_ctx(ctx) == 0);
    assert(bs_registry_facade_current_phase(facade) == BS_REGISTRY_PHASE_P1);

    /* Enter P2 before adapter plugin registration */
    assert(bs_registry_facade_advance_phase(facade, BS_REGISTRY_PHASE_P2) == BS_REGISTRY_OK);
    assert(bs_registry_facade_current_phase(facade) == BS_REGISTRY_PHASE_P2);

    /* Step 3: P2 adapter plugin declaration + hub + bind */
    PathEntry plugin{};
    plugin.source          = BS_PATH_ENTRY_PLUGIN;
    plugin.manifest_ref    = "adapter-manifest";
    plugin.type_constraint = nullptr;

    assert(bs_registry_facade_register_declaration(facade, "/adapter/plugin/test_plugin",
                                                   &plugin) == BS_REGISTRY_OK);
    assert(bs_registry_facade_register_hub_mapping(facade, "adapter.plugin.test_plugin",
                                                   "/adapter/plugin/test_plugin",
                                                   0) == BS_REGISTRY_OK);
    assert(bs_registry_facade_bind_instance(facade, "/adapter/plugin/test_plugin",
                                            reinterpret_cast<void*>(g_plugin_dispatch)) ==
           BS_REGISTRY_OK);

    /* Step 4: freeze before IR gate consumes IR */
    assert(bs_adapter_registry_bootstrap_freeze_ctx(ctx) == 0);
    assert(bs_registry_facade_current_phase(facade) == BS_REGISTRY_PHASE_FROZEN);

    /* Step 5: IR gate via requirement list (not stored in PathRegistry) */
    IRRequirementList* active = bs_adapter_requirement_filter_merge_activation(nullptr);
    assert(active != nullptr);

    IRInstructionList* list = bs_ir_instruction_list_create();
    IRInstruction*     ok   = bs_ir_instruction_create("test", "n1");
    assert(bs_ir_instruction_list_add(list, ok) == 0);
    assert(bs_adapter_requirement_filter_verify_instructions(list, active) == 0);

    IRInstruction* bad = bs_ir_instruction_create("not_in_manifest", "n2");
    assert(bs_ir_instruction_list_add(list, bad) == 0);
    assert(bs_adapter_requirement_filter_verify_instructions(list, active) == 1);

    bs_ir_instruction_list_destroy(list);
    bs_requirement_list_free(active);

    /* Step 6: resolve and use bindings (logical_id + canonical_path) */
    Binding gate{};
    assert(bs_registry_facade_resolve(facade, "kernel.ir.builtin_gate", &gate) == BS_REGISTRY_OK);
    int* gate_storage = static_cast<int*>(gate.impl);
    assert(gate_storage != nullptr);
    const int gate_before = *gate_storage;
    *gate_storage         = gate_before + 10;
    assert(*static_cast<int*>(gate.impl) == gate_before + 10);

    Binding plug{};
    assert(bs_registry_facade_resolve(facade, "adapter.plugin.test_plugin", &plug) ==
           BS_REGISTRY_OK);
    auto plug_fn = reinterpret_cast<int (*)(int)>(plug.impl);
    assert(plug_fn(41) == 42);

    assert(bs_registry_facade_resolve(facade, "/adapter/plugin/test_plugin", &plug) ==
           BS_REGISTRY_OK);
    assert(reinterpret_cast<int (*)(int)>(plug.impl)(0) == 1);

    Binding io_local{};
    assert(bs_registry_facade_resolve(facade, "adapter.io.local", &io_local) == BS_REGISTRY_OK);
    assert(io_local.impl != nullptr);

    /* Step 7: post-freeze structural writes must fail */
    assert(bs_registry_facade_register_declaration(facade, "/adapter/plugin/after_freeze",
                                                   &plugin) == BS_REGISTRY_ERR_FROZEN);
    assert(bs_registry_facade_bind_instance(facade, "/adapter/plugin/after_freeze", &plugin) ==
           BS_REGISTRY_ERR_FROZEN);

    bs_adapter_registry_shutdown_log();
    bs_adapter_attach_ctx_destroy(ctx);
    return 0;
}
