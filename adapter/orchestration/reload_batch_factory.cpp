#include "bs/adapter/orchestration/reload_batch_factory.h"

static ReloadBatchController* default_create(unsigned max_inflight, void* user_ctx)
{
    (void)user_ctx;
    return bs_reload_batch_controller_create(max_inflight);
}

static void default_destroy(ReloadBatchController* ctrl, void* user_ctx)
{
    (void)user_ctx;
    bs_reload_batch_controller_destroy(ctrl);
}

static ReloadBatchControllerFactory g_default_factory = {
    default_create,
    default_destroy,
    nullptr,
};

const ReloadBatchControllerFactory* bs_adapter_reload_batch_default_factory(void)
{
    return &g_default_factory;
}

int bs_adapter_reload_batch_register_factory(RegistryFacade* facade)
{
    if (!facade)
        return -1;

    PathEntry entry{};
    entry.source          = BS_PATH_ENTRY_PLUGIN;
    entry.manifest_ref    = "orch-reload";
    entry.type_constraint = "reload_batch";

    if (bs_registry_facade_register_declaration(facade, "/adapter/orchestration/reload_batch",
                                                 &entry) != BS_REGISTRY_OK)
        return -1;

    if (bs_registry_facade_register_hub_mapping(
            facade, "adapter.orchestration.reload_batch", "/adapter/orchestration/reload_batch",
            0) != BS_REGISTRY_OK)
        return -1;

    if (bs_registry_facade_bind_instance(facade, "/adapter/orchestration/reload_batch",
                                         &g_default_factory) != BS_REGISTRY_OK)
        return -1;

    return 0;
}

ReloadBatchController* bs_reload_batch_controller_create_from_binding(const Binding* binding,
                                                                      unsigned max_inflight)
{
    if (!binding || !binding->impl)
        return nullptr;
    const auto* factory = static_cast<const ReloadBatchControllerFactory*>(binding->impl);
    if (!factory->create)
        return nullptr;
    return factory->create(max_inflight, factory->user_ctx);
}

void bs_reload_batch_controller_destroy_from_binding(const Binding* binding,
                                                   ReloadBatchController* ctrl)
{
    if (!binding || !binding->impl || !ctrl)
        return;
    const auto* factory = static_cast<const ReloadBatchControllerFactory*>(binding->impl);
    if (factory->destroy)
        factory->destroy(ctrl, factory->user_ctx);
}
