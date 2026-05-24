#include "bs/adapter/attach_context.h"
#include "bs/adapter/attach_runtime.h"
#include "bs/adapter/log/log_bus.h"
#include "bs/adapter/plugin/plugin_loader.h"
#include "bs/adapter/registry_bootstrap.h"
#include "bs/adapter/requirement_filter.h"

static BsAdapterStateNotifierFn g_state_notifier      = nullptr;
static void*                    g_state_notifier_user = nullptr;

void bs_adapter_registry_register_state_notifier(BsAdapterStateNotifierFn fn, void* user_data)
{
    g_state_notifier      = fn;
    g_state_notifier_user = user_data;
}

void bs_adapter_registry_clear_state_notifier(void)
{
    g_state_notifier      = nullptr;
    g_state_notifier_user = nullptr;
}

void bs_adapter_registry_shutdown_log(void)
{
    bs_adapter_log_shutdown_if_bound();
}

static void invoke_state_notifier(RegistryFacade* facade)
{
    if (g_state_notifier && facade)
        g_state_notifier(facade, g_state_notifier_user);
}

static AttachContext* facade_legacy_ctx(RegistryFacade* facade)
{
    AttachContext* legacy = bs_attach_context_legacy_bootstrap();
    bs_attach_context_use_external_registry(legacy, facade);
    return legacy;
}

int bs_adapter_registry_bootstrap_begin(RegistryFacade* facade)
{
    if (!facade)
        return -1;

    AttachContext* legacy  = facade_legacy_ctx(facade);
    AttachContext* log_ctx = bs_attach_context_get_active();
    if (!log_ctx)
    {
        bs_attach_context_set_active(legacy);
        log_ctx = legacy;
    }

    if (bs_adapter_requirement_filter_validate_builtin() != 0)
        return -1;

    if (bs_registry_facade_advance_phase(facade, BS_REGISTRY_PHASE_P1) != BS_REGISTRY_OK)
        return -1;

    if (bs_adapter_log_bind_spdlog_bus() != 0)
        return -1;
    bs_attach_context_set_log_bus_bound(log_ctx, 1);
    bs_adapter_attach_mark_log_ready(1);

    static int builtin_gate_stub = 1;

    PathEntry gate{};
    gate.source          = BS_PATH_ENTRY_BUILTIN;
    gate.manifest_ref    = "builtin";
    gate.type_constraint = "ir_gate";

    if (bs_registry_facade_register_declaration(facade, "/kernel/ir/builtin_gate", &gate) !=
        BS_REGISTRY_OK)
        return -1;

    if (bs_registry_facade_register_hub_mapping(facade, "kernel.ir.builtin_gate",
                                                "/kernel/ir/builtin_gate", 0) != BS_REGISTRY_OK)
        return -1;

    if (bs_registry_facade_bind_instance(facade, "/kernel/ir/builtin_gate", &builtin_gate_stub) !=
        BS_REGISTRY_OK)
        return -1;

    PathEntry log_bus_entry{};
    log_bus_entry.source          = BS_PATH_ENTRY_BUILTIN;
    log_bus_entry.manifest_ref    = "builtin";
    log_bus_entry.type_constraint = "log_bus";

    if (bs_registry_facade_register_declaration(facade, "/kernel/log/bus", &log_bus_entry) !=
        BS_REGISTRY_OK)
        return -1;

    if (bs_registry_facade_register_hub_mapping(facade, "kernel.log.bus", "/kernel/log/bus", 0) !=
        BS_REGISTRY_OK)
        return -1;

    static BsLogBusOps* bound_ops_marker = nullptr;
    if (bs_registry_facade_bind_instance(facade, "/kernel/log/bus",
                                         reinterpret_cast<void*>(&bound_ops_marker)) !=
        BS_REGISTRY_OK)
        return -1;

    return 0;
}

int bs_adapter_registry_bootstrap_register_standard_io(RegistryFacade* facade)
{
    if (!facade)
        return -1;
    return bs_adapter_plugin_loader_load_one(facade_legacy_ctx(facade), "io-standard");
}

int bs_adapter_registry_bootstrap_freeze(RegistryFacade* facade)
{
    if (!facade)
        return -1;
    if (!bs_adapter_attach_is_log_ready())
        return -1;

    AttachContext* legacy = facade_legacy_ctx(facade);
    if (!bs_attach_context_get_active())
        bs_attach_context_set_active(legacy);

    if (bs_adapter_plugin_loader_load_all(legacy, nullptr) != 0)
        return -1;

    /* R5-02 / IMPL-05-01: builtin ir_gate registered at bootstrap_begin (P1, before freeze). */
    if (bs_registry_facade_freeze(facade) != BS_REGISTRY_OK)
        return -1;
    invoke_state_notifier(facade);
    return 0;
}

int bs_adapter_registry_bootstrap_begin_ctx(AttachContext* ctx)
{
    RegistryFacade* facade = bs_attach_context_registry(ctx);
    if (!facade)
        return -1;

    bs_attach_context_set_active(ctx);

    if (bs_adapter_registry_bootstrap_begin(facade) != 0)
        return -1;

    bs_attach_context_set_log_bus_bound(ctx, bs_adapter_attach_is_log_ready() ? 1 : 0);
    return 0;
}

int bs_adapter_registry_bootstrap_register_standard_io_ctx(AttachContext* ctx)
{
    if (!ctx)
        return -1;
    return bs_adapter_plugin_loader_load_one(ctx, "io-standard");
}

int bs_adapter_registry_bootstrap_freeze_ctx(AttachContext* ctx)
{
    RegistryFacade* facade = bs_attach_context_registry(ctx);
    if (!facade)
        return -1;
    if (!bs_attach_context_is_log_bus_bound(ctx))
        return -1;

    if (bs_adapter_plugin_loader_load_all(ctx, nullptr) != 0)
        return -1;

    if (bs_registry_facade_freeze(facade) != BS_REGISTRY_OK)
        return -1;
    invoke_state_notifier(facade);
    return 0;
}
