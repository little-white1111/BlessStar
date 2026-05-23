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

EventBus* EventBus_Create()
{
    return new EventBus();
}

void EventBus_Destroy(EventBus* bus)
{
    if (!bus)
        return;
    for (ConfigEvent* ev : bus->pending)
        ConfigEvent_Destroy(ev);
    delete bus;
}

int EventBus_Subscribe(EventBus* bus, const char* path, EventListener listener, void* userData)
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

int EventBus_Unsubscribe(EventBus* bus, const char* path, EventListener listener)
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

int EventBus_Publish(EventBus* bus, const ConfigEvent* event)
{
    if (!bus || !event || !event->configPath)
        return -1;

    ConfigEvent* copy = ConfigEvent_Create(event->configPath, event->type, event->fromState,
                                           event->toState, event->version);
    if (!copy)
        return -1;

    std::unique_lock<std::shared_mutex> lock(bus->mutex);
    bus->pending.push_back(copy);
    return 0;
}

int EventBus_Drain(EventBus* bus)
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
            ConfigEvent_Destroy(event);
            continue;
        }

        std::shared_lock<std::shared_mutex> lock(bus->mutex);
        auto                                it = bus->listeners.find(event->configPath);
        if (it == bus->listeners.end())
        {
            ConfigEvent_Destroy(event);
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

        ConfigEvent_Destroy(event);
    }

    return 0;
}
