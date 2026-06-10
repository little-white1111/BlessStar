#include "bs/kernel/state/StateBus.h"
#include "bs/kernel/state/StateSnapshotRcu.h"

#include <cstdlib>
#include <cstring>
#include <ctime>

#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
struct PathSeqlock
{
    std::atomic<uint64_t>                         seq{0};
    std::shared_ptr<const BsStateSnapshotPayload> ptr;

    void publish(std::shared_ptr<const BsStateSnapshotPayload> next)
    {
        seq.fetch_add(1, std::memory_order_acq_rel);
        std::atomic_store_explicit(&ptr, std::move(next), std::memory_order_release);
        seq.fetch_add(1, std::memory_order_acq_rel);
    }

    std::shared_ptr<const BsStateSnapshotPayload> pin() const
    {
        for (;;)
        {
            const uint64_t s0 = seq.load(std::memory_order_acquire);
            if (s0 & 1u)
                continue;
            auto snap = std::atomic_load_explicit(&ptr, std::memory_order_acquire);
            std::atomic_thread_fence(std::memory_order_acquire);
            if (seq.load(std::memory_order_acquire) == s0)
                return snap;
        }
    }
};

struct StateEntryImpl
{
    StateEntry  pub{};
    PathSeqlock rcu;
};

static std::shared_ptr<const BsStateSnapshotPayload>
make_payload(ConfigState state, uint64_t version, const void* data, size_t dataSize)
{
    auto payload     = std::make_shared<BsStateSnapshotPayload>();
    payload->state   = state;
    payload->version = version;
    if (data && dataSize > 0)
    {
        const auto* bytes = static_cast<const uint8_t*>(data);
        payload->bytes.assign(bytes, bytes + dataSize);
    }
    return payload;
}

static void publish_entry_snapshot(StateEntryImpl* impl, ConfigState state, uint64_t version,
                                   const void* data, size_t dataSize)
{
    if (!impl)
        return;
    impl->rcu.publish(make_payload(state, version, data, dataSize));
}

} // namespace

struct StateBus
{
    std::unordered_map<std::string, StateEntryImpl*> entries;
    std::shared_mutex                                mutex;
};

StateBus* bs_state_bus_create()
{
    return new StateBus();
}

void bs_state_bus_destroy(StateBus* bus)
{
    if (!bus)
        return;
    for (auto& pair : bus->entries)
    {
        StateEntryImpl* impl = pair.second;
        if (impl->pub.path)
            free((void*)impl->pub.path);
        if (impl->pub.dataSnapshot)
            free((void*)impl->pub.dataSnapshot);
        delete impl;
    }
    delete bus;
}

int bs_state_bus_set_state(StateBus* bus, const char* path, ConfigState state, const void* data,
                           size_t dataSize)
{
    if (!bus || !path)
        return -1;

    std::unique_lock<std::shared_mutex> lock(bus->mutex);

    auto            it   = bus->entries.find(path);
    StateEntryImpl* impl = nullptr;

    if (it == bus->entries.end())
    {
        impl                   = new StateEntryImpl();
        impl->pub.path         = strdup(path);
        impl->pub.dataSnapshot = nullptr;
        impl->pub.dataSize     = 0;
        impl->pub.version      = 0;
        impl->pub.next         = nullptr;
        bus->entries[path]     = impl;
    }
    else
    {
        impl = it->second;
    }

    StateEntry* entry = &impl->pub;
    entry->state      = state;
    entry->version++;
    entry->timestamp = (uint64_t)time(nullptr);

    if (entry->dataSnapshot)
        free((void*)entry->dataSnapshot);
    entry->dataSnapshot = nullptr;
    entry->dataSize     = 0;

    if (data && dataSize > 0)
    {
        entry->dataSnapshot = malloc(dataSize);
        if (entry->dataSnapshot)
        {
            memcpy((void*)entry->dataSnapshot, data, dataSize);
            entry->dataSize = dataSize;
        }
    }

    publish_entry_snapshot(impl, entry->state, entry->version, entry->dataSnapshot,
                           entry->dataSize);
    return 0;
}

int bs_state_bus_get_state(StateBus* bus, const char* path, StateEntry** entry)
{
    if (!bus || !path || !entry)
        return -1;

    std::shared_lock<std::shared_mutex> lock(bus->mutex);

    auto it = bus->entries.find(path);
    if (it == bus->entries.end())
    {
        *entry = nullptr;
        return -2;
    }

    *entry = &it->second->pub;
    return 0;
}

int bs_state_bus_get_snapshot(StateBus* bus, const char* path, void** data, size_t* size)
{
    if (!bus || !path || !data || !size)
        return -1;

    const auto pinned = bs_state_bus_pin_snapshot(bus, path);
    if (!pinned)
        return -2;
    if (pinned->bytes.empty())
        return -2;

    *size = pinned->bytes.size();
    *data = malloc(*size);
    if (!*data)
        return -3;
    memcpy(*data, pinned->bytes.data(), *size);
    return 0;
}

StateEntry* bs_state_bus_get_all_entries(StateBus* bus, size_t* count)
{
    if (!bus || !count)
        return nullptr;

    std::shared_lock<std::shared_mutex> lock(bus->mutex);

    *count = bus->entries.size();
    if (bus->entries.empty())
        return nullptr;

    std::vector<StateEntryImpl*> tempEntries;
    tempEntries.reserve(bus->entries.size());
    for (const auto& pair : bus->entries)
        tempEntries.push_back(pair.second);

    StateEntry* result = (StateEntry*)malloc(bus->entries.size() * sizeof(StateEntry));
    if (!result)
        return nullptr;

    for (size_t i = 0; i < bus->entries.size(); i++)
    {
        StateEntry* src     = &tempEntries[i]->pub;
        result[i].path      = strdup(src->path);
        result[i].state     = src->state;
        result[i].version   = src->version;
        result[i].timestamp = src->timestamp;
        result[i].dataSize  = src->dataSize;
        if (src->dataSnapshot && src->dataSize > 0)
        {
            result[i].dataSnapshot = malloc(src->dataSize);
            if (result[i].dataSnapshot)
                memcpy((void*)result[i].dataSnapshot, src->dataSnapshot, src->dataSize);
        }
        else
        {
            result[i].dataSnapshot = nullptr;
        }
        result[i].next = nullptr;
    }

    return result;
}

void bs_state_bus_free_entries(StateEntry* entries, size_t count)
{
    if (!entries)
        return;
    for (size_t i = 0; i < count; i++)
    {
        free((void*)entries[i].path);
        if (entries[i].dataSnapshot)
            free((void*)entries[i].dataSnapshot);
    }
    free(entries);
}

std::shared_ptr<const BsStateSnapshotPayload> bs_state_bus_pin_snapshot(StateBus*   bus,
                                                                        const char* path)
{
    if (!bus || !path)
        return {};

    StateEntryImpl* impl = nullptr;
    {
        std::shared_lock<std::shared_mutex> lock(bus->mutex);
        const auto                          it = bus->entries.find(path);
        if (it == bus->entries.end())
            return {};
        impl = it->second;
    }
    return impl->rcu.pin();
}
