#include "bs/kernel/state/ShardedStateBus.h"
#include "bs/kernel/state/StateBus.h"

#include <cstdlib>
#include <cstring>

#include <atomic>
#include <functional>
#include <vector>

struct ShardedStateBus
{
    std::vector<StateBus*> shards;
    size_t                 num_shards;
    std::atomic<uint64_t>  total_operations;
};

static inline size_t hash_string(const char* str)
{
    size_t hash = 5381;
    int    c;
    while ((c = *str++))
    {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

static inline size_t get_shard_index(const char* path, size_t num_shards)
{
    return hash_string(path) % num_shards;
}

ShardedStateBus* bs_sharded_state_bus_create(size_t num_shards)
{
    if (num_shards == 0)
        num_shards = 16;

    ShardedStateBus* bus = new ShardedStateBus();
    bus->num_shards      = num_shards;
    bus->total_operations.store(0);
    bus->shards.resize(num_shards);

    for (size_t i = 0; i < num_shards; i++)
    {
        bus->shards[i] = bs_state_bus_create();
    }

    return bus;
}

void bs_sharded_state_bus_destroy(ShardedStateBus* bus)
{
    if (!bus)
        return;
    for (size_t i = 0; i < bus->num_shards; i++)
    {
        bs_state_bus_destroy(bus->shards[i]);
    }
    delete bus;
}

int bs_sharded_state_bus_set_state(ShardedStateBus* bus, const char* path, ConfigState state,
                                   const void* data, size_t dataSize)
{
    if (!bus || !path)
        return -1;

    size_t index  = get_shard_index(path, bus->num_shards);
    int    result = bs_state_bus_set_state(bus->shards[index], path, state, data, dataSize);

    if (result == 0)
    {
        bus->total_operations.fetch_add(1);
    }

    return result;
}

int bs_sharded_state_bus_get_state(ShardedStateBus* bus, const char* path, StateEntry** entry)
{
    if (!bus || !path || !entry)
        return -1;

    size_t index = get_shard_index(path, bus->num_shards);
    return bs_state_bus_get_state(bus->shards[index], path, entry);
}

int bs_sharded_state_bus_get_snapshot(ShardedStateBus* bus, const char* path, void** data,
                                      size_t* size)
{
    if (!bus || !path || !data || !size)
        return -1;

    size_t index = get_shard_index(path, bus->num_shards);
    return bs_state_bus_get_snapshot(bus->shards[index], path, data, size);
}

StateEntry* bs_sharded_state_bus_get_all_entries(ShardedStateBus* bus, size_t* count)
{
    if (!bus || !count)
        return nullptr;

    *count = 0;
    std::vector<StateEntry> all_entries;

    for (size_t i = 0; i < bus->num_shards; i++)
    {
        size_t      shard_count = 0;
        StateEntry* entries     = bs_state_bus_get_all_entries(bus->shards[i], &shard_count);
        if (entries && shard_count > 0)
        {
            for (size_t j = 0; j < shard_count; j++)
            {
                StateEntry dst{};
                dst.path      = entries[j].path ? strdup(entries[j].path) : nullptr;
                dst.state     = entries[j].state;
                dst.version   = entries[j].version;
                dst.timestamp = entries[j].timestamp;
                dst.dataSize  = entries[j].dataSize;
                if (entries[j].dataSnapshot && entries[j].dataSize > 0)
                {
                    dst.dataSnapshot = malloc(entries[j].dataSize);
                    if (dst.dataSnapshot)
                        memcpy((void*)dst.dataSnapshot, entries[j].dataSnapshot,
                               entries[j].dataSize);
                }
                dst.next = nullptr;
                all_entries.push_back(dst);
            }
            bs_state_bus_free_entries(entries, shard_count);
        }
    }

    *count = all_entries.size();
    if (all_entries.empty())
        return nullptr;

    StateEntry* result = (StateEntry*)malloc(all_entries.size() * sizeof(StateEntry));
    if (!result)
    {
        for (auto& e : all_entries)
        {
            if (e.path)
                free((void*)e.path);
            if (e.dataSnapshot)
                free((void*)e.dataSnapshot);
        }
        return nullptr;
    }

    for (size_t i = 0; i < all_entries.size(); i++)
        result[i] = all_entries[i];
    return result;
}

void bs_sharded_state_bus_free_entries(StateEntry* entries, size_t count)
{
    if (!entries)
        return;
    for (size_t i = 0; i < count; i++)
    {
        free((void*)entries[i].path);
        if (entries[i].dataSnapshot)
        {
            free((void*)entries[i].dataSnapshot);
        }
    }
    free(entries);
}

uint64_t bs_sharded_state_bus_get_total_operations(ShardedStateBus* bus)
{
    if (!bus)
        return 0;
    return bus->total_operations.load();
}

size_t bs_sharded_state_bus_get_shard_count(ShardedStateBus* bus)
{
    if (!bus)
        return 0;
    return bus->num_shards;
}
