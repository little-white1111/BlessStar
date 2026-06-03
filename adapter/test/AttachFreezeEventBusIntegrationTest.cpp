/**
 * IMPL-08-09 / R8-07 / B-04: attach freeze notifies via ConfigManager EventBus (not ad-hoc bus).
 * Expect at least one CONFIG_EVENT_ENTER_ACTIVE (LOADING->ACTIVE) on
 * BS_ADAPTER_CONFIG_PATH_ATTACH_FROZEN.
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
    if (!event || !event->configPath ||
        std::strcmp(event->configPath, BS_ADAPTER_CONFIG_PATH_ATTACH_FROZEN) != 0)
        return;
    if (event->type == CONFIG_EVENT_ENTER_ACTIVE && event->fromState == CONFIG_STATE_LOADING &&
        event->toState == CONFIG_STATE_ACTIVE)
        ++g_listener_hits;
}

int main()
{
    AttachContext* ctx = bs_adapter_attach_ctx_create();
    assert(ctx != nullptr);

    EventBus* bus = bs_adapter_attach_config_event_bus(ctx);
    assert(bus != nullptr);
    assert(bs_event_bus_subscribe(bus, BS_ADAPTER_CONFIG_PATH_ATTACH_FROZEN, listener, nullptr) ==
           0);

    assert(bs_adapter_registry_bootstrap_begin_ctx(ctx) == 0);
    assert(bs_adapter_registry_bootstrap_register_standard_io_ctx(ctx) == 0);
    assert(bs_adapter_registry_bootstrap_freeze_ctx(ctx) == 0);

    assert(g_listener_hits >= 1);

    ConfigState st = CONFIG_STATE_INITIAL;
    assert(bs_adapter_attach_config_get_state(ctx, BS_ADAPTER_CONFIG_PATH_ATTACH_FROZEN, &st) == 0);
    assert(st == CONFIG_STATE_ACTIVE);

    bs_adapter_attach_ctx_destroy(ctx);
    bs_adapter_registry_shutdown_log();
    return 0;
}
