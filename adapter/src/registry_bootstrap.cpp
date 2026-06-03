#include "bs/adapter/attach_config.h"
#include "bs/adapter/attach_context.h"
#include "bs/adapter/attach_runtime.h"
#include "bs/adapter/log/log_bus.h"
#include "bs/adapter/plugin/plugin_loader.h"
#include "bs/adapter/registry_bootstrap.h"
#include "bs/adapter/requirement_filter.h"

#include "attach_context_internal.h"

void bs_adapter_registry_shutdown_log(void)
{
    bs_adapter_log_shutdown_if_bound();
}

static AttachContext* active_ctx_for_facade(RegistryFacade* facade)
{
    if (!facade)
        return nullptr;
    AttachContext* active = bs_adapter_attach_ctx_get_active();
    if (!active || bs_adapter_attach_ctx_registry(active) != facade)
        return nullptr;
    return active;
}

static int bind_bootstrap_log_bus(void)
{
    return bs_adapter_log_bind_spdlog_bus();
}

static int bootstrap_begin_impl(AttachContext* log_ctx, RegistryFacade* facade)
{
    if (!log_ctx || !facade)
        return -1;

    if (bs_adapter_requirement_filter_validate_builtin() != 0)
        return -1;

    if (bs_registry_facade_advance_phase(facade, BS_REGISTRY_PHASE_P1) != BS_REGISTRY_OK)
        return -1;

    if (bind_bootstrap_log_bus() != 0)
        return -1;
    bs_adapter_attach_ctx_set_log_bus_bound(log_ctx, 1);
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

    if (bs_adapter_attach_ctx_bind_default_pipeline_registry(log_ctx, facade) != 0)
        return -1;

    return 0;
}

int bs_adapter_registry_bootstrap_begin(RegistryFacade* facade)
{
    AttachContext* active = active_ctx_for_facade(facade);
    if (!active)
        return -1;
    return bootstrap_begin_impl(active, facade);
}

int bs_adapter_registry_bootstrap_register_standard_io(RegistryFacade* facade)
{
    AttachContext* active = active_ctx_for_facade(facade);
    if (!active)
        return -1;
    return bs_adapter_plugin_loader_load_one(active, "io-standard");
}

int bs_adapter_registry_bootstrap_freeze(RegistryFacade* facade)
{
    AttachContext* active = active_ctx_for_facade(facade);
    if (!active)
        return -1;
    return bs_adapter_registry_bootstrap_freeze_ctx(active);
}

int bs_adapter_registry_bootstrap_begin_ctx(AttachContext* ctx)
{
    RegistryFacade* facade = bs_adapter_attach_ctx_registry(ctx);
    if (!facade)
        return -1;

    bs_adapter_attach_ctx_set_active(ctx);

    if (bootstrap_begin_impl(ctx, facade) != 0)
        return -1;

    bs_adapter_attach_ctx_set_log_bus_bound(ctx, bs_adapter_attach_is_log_ready() ? 1 : 0);
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
    RegistryFacade* facade = bs_adapter_attach_ctx_registry(ctx);
    if (!facade)
        return -1;
    if (!bs_adapter_attach_ctx_is_log_bus_bound(ctx))
        return -1;

    if (bs_adapter_plugin_loader_load_all(ctx, nullptr) != 0)
        return -1;

    if (bs_registry_facade_freeze(facade) != BS_REGISTRY_OK)
        return -1;
    if (bs_adapter_attach_notify_registry_frozen(ctx) != 0)
        return -1;
    if (bs_adapter_attach_ctx_start_kernel(ctx) != 0)
        return -1;
    return 0;
}
