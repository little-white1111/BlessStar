#include "bs/kernel/common/bs_reentrancy.h"
#include "bs/kernel/state/ConfigEvent.h"
#include "bs/kernel/state/EventBus.h"

#include <cstdlib>
#include <cstring>

#include <deque>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct ListenerEntry
{
    EventListener listener;
    void*         userData;
};

struct EventBus
{
    std::unordered_map<std::string, std::vector<ListenerEntry>> listeners;
    std::shared_mutex                                           mutex;
    std::deque<ConfigEvent*>                                    pending;
};

EventBus* bs_event_bus_create()
{
    return new EventBus();
}

void bs_event_bus_destroy(EventBus* bus)
{
    if (!bus)
        return;
    for (ConfigEvent* ev : bus->pending)
        bs_config_event_destroy(ev);
    delete bus;
}

int bs_event_bus_subscribe(EventBus* bus, const char* path, EventListener listener, void* userData)
{
    if (!bus || !path || !listener)
        return -1;

    std::unique_lock<std::shared_mutex> lock(bus->mutex);

    ListenerEntry entry;
    entry.listener = listener;
    entry.userData = userData;

    bus->listeners[path].push_back(entry);

    return 0;
}

int bs_event_bus_unsubscribe(EventBus* bus, const char* path, EventListener listener)
{
    if (!bus || !path || !listener)
        return -1;

    std::unique_lock<std::shared_mutex> lock(bus->mutex);

    auto it = bus->listeners.find(path);
    if (it == bus->listeners.end())
        return -2;

    auto& entries = it->second;
    for (auto entryIt = entries.begin(); entryIt != entries.end(); ++entryIt)
    {
        if (entryIt->listener == listener)
        {
            entries.erase(entryIt);
            break;
        }
    }

    return 0;
}

int bs_event_bus_publish(EventBus* bus, const ConfigEvent* event)
{
    if (!bus || !event || !event->configPath)
        return -1;

    ConfigEvent* copy = bs_config_event_create(event->configPath, event->type, event->fromState,
                                               event->toState, event->version);
    if (!copy)
        return -1;

    std::unique_lock<std::shared_mutex> lock(bus->mutex);
    bus->pending.push_back(copy);
    return 0;
}

int bs_event_bus_drain(EventBus* bus)
{
    if (!bus)
        return -1;

    std::deque<ConfigEvent*> batch;
    {
        std::unique_lock<std::shared_mutex> lock(bus->mutex);
        batch.swap(bus->pending);
    }

    for (ConfigEvent* event : batch)
    {
        if (!event || !event->configPath)
        {
            bs_config_event_destroy(event);
            continue;
        }

        std::shared_lock<std::shared_mutex> lock(bus->mutex);
        auto                                it = bus->listeners.find(event->configPath);
        if (it == bus->listeners.end())
        {
            bs_config_event_destroy(event);
            continue;
        }

        const auto listeners_copy = it->second;
        lock.unlock();

        for (const auto& entry : listeners_copy)
        {
            bs_reentrancy_enter_state_callback();
            entry.listener(event, entry.userData);
            bs_reentrancy_leave_state_callback();
        }

        bs_config_event_destroy(event);
    }

    return 0;
}
