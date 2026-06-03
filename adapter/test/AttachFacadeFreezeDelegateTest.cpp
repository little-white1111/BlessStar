/**
 * IMPL-17-ATTACH-01/02 / T7: facade freeze delegates to active ctx; mismatch returns -1.
 */

#include "bs/kernel/state/ConfigEvent.h"
#include "bs/kernel/state/ConfigState.h"

#include "bs/adapter/attach_config.h"
#include "bs/adapter/attach_context.h"
#include "bs/adapter/registry_bootstrap.h"

#include <cassert>
#include <cstring>

static int g_listener_hits = 0;

static void listener(const ConfigEvent* event, void* /*user*/)
{
    if (event && event->configPath &&
        std::strcmp(event->configPath, BS_ADAPTER_CONFIG_PATH_ATTACH_FROZEN) == 0)
        ++g_listener_hits;
}

int main()
{
    RegistryFacade* orphan = bs_registry_facade_create();
    assert(orphan != nullptr);
    assert(bs_adapter_registry_bootstrap_freeze(orphan) == -1);
    bs_registry_facade_destroy(orphan);

    AttachContext* ctx_a = bs_adapter_attach_ctx_create();
    AttachContext* ctx_b = bs_adapter_attach_ctx_create();
    assert(ctx_a && ctx_b);

    bs_adapter_attach_ctx_set_active(ctx_a);
    RegistryFacade* facade_a = bs_adapter_attach_ctx_registry(ctx_a);
    assert(facade_a != nullptr);

    EventBus* bus = bs_adapter_attach_config_event_bus(ctx_a);
    assert(bus != nullptr);
    assert(bs_event_bus_subscribe(bus, BS_ADAPTER_CONFIG_PATH_ATTACH_FROZEN, listener, nullptr) ==
           0);

    assert(bs_adapter_registry_bootstrap_begin_ctx(ctx_a) == 0);
    assert(bs_adapter_registry_bootstrap_register_standard_io_ctx(ctx_a) == 0);

    bs_adapter_attach_ctx_set_active(ctx_b);
    assert(bs_adapter_registry_bootstrap_freeze(facade_a) == -1);

    bs_adapter_attach_ctx_set_active(ctx_a);
    assert(bs_adapter_registry_bootstrap_freeze(facade_a) == 0);
    assert(g_listener_hits >= 1);

    ConfigState st = CONFIG_STATE_INITIAL;
    assert(bs_adapter_attach_config_get_state(ctx_a, BS_ADAPTER_CONFIG_PATH_ATTACH_FROZEN, &st) ==
           0);
    assert(st == CONFIG_STATE_ACTIVE);

    bs_adapter_attach_ctx_destroy(ctx_a);
    bs_adapter_attach_ctx_destroy(ctx_b);
    bs_adapter_registry_shutdown_log();
    return 0;
}
