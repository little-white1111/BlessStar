/**
 * IMPL-08-09 / R8-07 A: attach freeze may notify external EventBus without ConfigManager in
 * bootstrap.
 */

#include "bs/kernel/state/ConfigEvent.h"
#include "bs/kernel/state/EventBus.h"

#include "bs/adapter/attach_context.h"
#include "bs/adapter/registry_bootstrap.h"

#include <cassert>
#include <cstring>

static EventBus* g_bus           = nullptr;
static int       g_listener_hits = 0;

static void on_attach_frozen(RegistryFacade* /*facade*/, void* user_data)
{
    auto*       bus = static_cast<EventBus*>(user_data);
    ConfigEvent ev{};
    ev.configPath = "/config/attach/frozen";
    ev.type       = CONFIG_EVENT_ENTER_ACTIVE;
    ev.fromState  = CONFIG_STATE_LOADING;
    ev.toState    = CONFIG_STATE_ACTIVE;
    ev.version    = 1;
    ev.timestamp  = 1;
    assert(EventBus_Publish(bus, &ev) == 0);
}

static void listener(const ConfigEvent* event, void* /*user*/)
{
    if (event && event->configPath && std::strcmp(event->configPath, "/config/attach/frozen") == 0)
        ++g_listener_hits;
}

int main()
{
    g_bus = EventBus_Create();
    assert(g_bus != nullptr);
    assert(EventBus_Subscribe(g_bus, "/config/attach/frozen", listener, nullptr) == 0);

    bs_adapter_registry_register_state_notifier(on_attach_frozen, g_bus);

    AttachContext* ctx = bs_attach_context_create();
    assert(ctx != nullptr);
    assert(bs_adapter_registry_bootstrap_begin_ctx(ctx) == 0);
    assert(bs_adapter_registry_bootstrap_register_standard_io_ctx(ctx) == 0);
    assert(bs_adapter_registry_bootstrap_freeze_ctx(ctx) == 0);

    assert(EventBus_Drain(g_bus) == 0);
    assert(g_listener_hits == 1);

    bs_adapter_registry_clear_state_notifier();
    bs_attach_context_destroy(ctx);
    EventBus_Destroy(g_bus);
    return 0;
}
