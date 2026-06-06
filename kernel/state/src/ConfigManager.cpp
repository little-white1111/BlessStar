#include "bs/kernel/state/ConfigManager.h"
#include "bs/kernel/state/EventBus.h"
#include "bs/kernel/state/StateBus.h"
#include "bs/kernel/state/WatchManager.h"

#include <cstring>

struct ConfigManager
{
    StateBus*                     state_bus;
    EventBus*                     event_bus;
    WatchManager*                 watch_manager;
    BsConfigManagerPhase2NotifyFn phase2_notify_fn;
    void*                         phase2_notify_user;
};

namespace
{
ConfigEventType event_type_for_state(ConfigState state)
{
    switch (state)
    {
    case CONFIG_STATE_INITIAL:
        return CONFIG_EVENT_ENTER_INITIAL;
    case CONFIG_STATE_LOADING:
        return CONFIG_EVENT_ENTER_LOADING;
    case CONFIG_STATE_ACTIVE:
        return CONFIG_EVENT_ENTER_ACTIVE;
    case CONFIG_STATE_UPDATING:
        return CONFIG_EVENT_ENTER_UPDATING;
    case CONFIG_STATE_ERROR:
        return CONFIG_EVENT_ENTER_ERROR;
    case CONFIG_STATE_CLOSED:
        return CONFIG_EVENT_ENTER_CLOSED;
    }
    return CONFIG_EVENT_ENTER_INITIAL;
}

int lookup_state(StateBus* bus, const char* path, ConfigState* state, bool* found)
{
    if (!bus || !path || !state || !found)
        return -1;

    StateEntry* entry = nullptr;
    const int   rc    = bs_state_bus_get_state(bus, path, &entry);
    if (rc == -2)
    {
        *found = false;
        *state = CONFIG_STATE_INITIAL;
        return 0;
    }
    if (rc != 0)
        return rc;

    *found = true;
    *state = entry->state;
    return 0;
}

int emit_transition(ConfigManager* cm, const char* path, ConfigState from, ConfigState to,
                    const void* data, size_t dataSize)
{
    /* Phase 1: commit state under StateBus lock (no listener callbacks yet). */
    if (bs_state_bus_set_state(cm->state_bus, path, to, data, dataSize) != 0)
        return -1;

    StateEntry* entry = nullptr;
    if (bs_state_bus_get_state(cm->state_bus, path, &entry) != 0 || !entry)
        return -1;

    ConfigEvent payload{};
    payload.configPath   = path;
    payload.type         = event_type_for_state(to);
    payload.fromState    = from;
    payload.toState      = to;
    payload.version      = entry->version;
    payload.timestamp    = entry->timestamp;
    const void* snapshot = entry->dataSnapshot;

    /* Phase 2: publish + drain + watch notify outside StateBus mutation. */
    if (bs_event_bus_publish(cm->event_bus, &payload) != 0)
        return -1;
    if (bs_event_bus_drain(cm->event_bus) != 0)
        return -1;

    if (cm->phase2_notify_fn)
        cm->phase2_notify_fn(cm, cm->watch_manager, path, payload.type, snapshot,
                             cm->phase2_notify_user);
    else
        bs_watch_manager_notify(cm->watch_manager, path, payload.type, snapshot);
    return 0;
}
} // namespace

ConfigManager* bs_config_manager_create()
{
    ConfigManager* cm = new ConfigManager();
    if (!cm)
        return nullptr;
    cm->state_bus          = bs_state_bus_create();
    cm->event_bus          = bs_event_bus_create();
    cm->watch_manager      = bs_watch_manager_create();
    cm->phase2_notify_fn   = nullptr;
    cm->phase2_notify_user = nullptr;
    if (!cm->state_bus || !cm->event_bus || !cm->watch_manager)
    {
        bs_config_manager_destroy(cm);
        return nullptr;
    }
    return cm;
}

void bs_config_manager_destroy(ConfigManager* cm)
{
    if (!cm)
        return;
    if (cm->watch_manager)
        bs_watch_manager_destroy(cm->watch_manager);
    if (cm->event_bus)
        bs_event_bus_destroy(cm->event_bus);
    if (cm->state_bus)
        bs_state_bus_destroy(cm->state_bus);
    delete cm;
}

int bs_config_manager_load_config(ConfigManager* cm, const char* path, const void* data,
                                  size_t dataSize)
{
    if (!cm || !path)
        return -1;
    if (dataSize > 0 && !data)
        return -1;

    ConfigState current = CONFIG_STATE_INITIAL;
    bool        found   = false;
    if (lookup_state(cm->state_bus, path, &current, &found) != 0)
        return -1;

    if (found && current != CONFIG_STATE_INITIAL && current != CONFIG_STATE_CLOSED)
        return -3;

    ConfigState from = current;
    if (emit_transition(cm, path, from, CONFIG_STATE_LOADING, nullptr, 0) != 0)
        return -1;

    from = CONFIG_STATE_LOADING;
    if (emit_transition(cm, path, from, CONFIG_STATE_ACTIVE, data, dataSize) != 0)
        return -1;

    return 0;
}

int bs_config_manager_reload_config(ConfigManager* cm, const char* path, const void* data,
                                    size_t dataSize)
{
    if (!cm || !path)
        return -1;
    if (dataSize > 0 && !data)
        return -1;

    ConfigState current = CONFIG_STATE_INITIAL;
    bool        found   = false;
    if (lookup_state(cm->state_bus, path, &current, &found) != 0)
        return -1;
    if (!found || (current != CONFIG_STATE_ACTIVE && current != CONFIG_STATE_ERROR))
        return -2;

    ConfigState from = current;
    if (emit_transition(cm, path, from, CONFIG_STATE_UPDATING, nullptr, 0) != 0)
        return -1;

    from = CONFIG_STATE_UPDATING;
    if (emit_transition(cm, path, from, CONFIG_STATE_ACTIVE, data, dataSize) != 0)
        return -1;

    return 0;
}

int bs_config_manager_unload_config(ConfigManager* cm, const char* path)
{
    if (!cm || !path)
        return -1;

    ConfigState current = CONFIG_STATE_INITIAL;
    bool        found   = false;
    if (lookup_state(cm->state_bus, path, &current, &found) != 0)
        return -1;
    if (!found || current == CONFIG_STATE_INITIAL)
        return -2;

    if (emit_transition(cm, path, current, CONFIG_STATE_CLOSED, nullptr, 0) != 0)
        return -1;

    return 0;
}

int bs_config_manager_hot_update(ConfigManager* cm, const char* path, const void* newData,
                                 size_t newDataSize)
{
    if (!cm || !path)
        return -1;
    if (newDataSize > 0 && !newData)
        return -1;

    ConfigState current = CONFIG_STATE_INITIAL;
    bool        found   = false;
    if (lookup_state(cm->state_bus, path, &current, &found) != 0)
        return -1;
    if (!found || current != CONFIG_STATE_ACTIVE)
        return -2;

    if (emit_transition(cm, path, current, CONFIG_STATE_UPDATING, newData, newDataSize) != 0)
        return -1;

    if (emit_transition(cm, path, CONFIG_STATE_UPDATING, CONFIG_STATE_ACTIVE, newData,
                        newDataSize) != 0)
        return -1;

    return 0;
}

int bs_config_manager_get_config_state(ConfigManager* cm, const char* path, ConfigState* state)
{
    if (!cm || !path || !state)
        return -1;

    StateEntry* entry = nullptr;
    const int   rc    = bs_state_bus_get_state(cm->state_bus, path, &entry);
    if (rc == -2)
        return -2;
    if (rc != 0 || !entry)
        return -1;

    *state = entry->state;
    return 0;
}

int bs_config_manager_get_config_snapshot(ConfigManager* cm, const char* path, void** data,
                                          size_t* size)
{
    if (!cm || !path || !data || !size)
        return -1;
    return bs_state_bus_get_snapshot(cm->state_bus, path, data, size);
}

int bs_config_manager_subscribe_state_change(ConfigManager* cm, const char* path,
                                             WatchCallback callback, void* userData)
{
    if (!cm || !path || !callback)
        return -1;
    return bs_watch_manager_add_watch(cm->watch_manager, path, callback, WATCH_MODE_PERSISTENT,
                                      userData);
}

int bs_config_manager_unsubscribe_state_change(ConfigManager* cm, const char* path,
                                               WatchCallback callback)
{
    if (!cm || !path || !callback)
        return -1;
    return bs_watch_manager_remove_watch(cm->watch_manager, path, callback);
}

StateBus* bs_config_manager_get_state_bus(ConfigManager* cm)
{
    return cm ? cm->state_bus : nullptr;
}

EventBus* bs_config_manager_get_event_bus(ConfigManager* cm)
{
    return cm ? cm->event_bus : nullptr;
}

WatchManager* bs_config_manager_get_watch_manager(ConfigManager* cm)
{
    return cm ? cm->watch_manager : nullptr;
}

void bs_config_manager_set_phase2_notify_hook(ConfigManager* cm, BsConfigManagerPhase2NotifyFn fn,
                                              void* user_data)
{
    if (!cm)
        return;
    cm->phase2_notify_fn   = fn;
    cm->phase2_notify_user = user_data;
}
