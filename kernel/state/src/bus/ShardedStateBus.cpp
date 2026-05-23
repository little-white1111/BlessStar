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

ShardedStateBus* ShardedStateBus_Create(size_t num_shards)
{
    if (num_shards == 0)
        num_shards = 16;

    ShardedStateBus* bus = new ShardedStateBus();
    bus->num_shards      = num_shards;
    bus->total_operations.store(0);
    bus->shards.resize(num_shards);

    for (size_t i = 0; i < num_shards; i++)
    {
        bus->shards[i] = StateBus_Create();
    }

    return bus;
}

void ShardedStateBus_Destroy(ShardedStateBus* bus)
{
    if (!bus)
        return;
    for (size_t i = 0; i < bus->num_shards; i++)
    {
        StateBus_Destroy(bus->shards[i]);
    }
    delete bus;
}

int ShardedStateBus_SetState(ShardedStateBus* bus, const char* path, ConfigState state,
                             const void* data, size_t dataSize)
{
    if (!bus || !path)
        return -1;

    size_t index  = get_shard_index(path, bus->num_shards);
    int    result = StateBus_SetState(bus->shards[index], path, state, data, dataSize);

    if (result == 0)
    {
        bus->total_operations.fetch_add(1);
    }

    return result;
}

int ShardedStateBus_GetState(ShardedStateBus* bus, const char* path, StateEntry** entry)
{
    if (!bus || !path || !entry)
        return -1;

    size_t index = get_shard_index(path, bus->num_shards);
    return StateBus_GetState(bus->shards[index], path, entry);
}

int ShardedStateBus_GetSnapshot(ShardedStateBus* bus, const char* path, void** data, size_t* size)
{
    if (!bus || !path || !data || !size)
        return -1;

    size_t index = get_shard_index(path, bus->num_shards);
    return StateBus_GetSnapshot(bus->shards[index], path, data, size);
}

StateEntry* ShardedStateBus_GetAllEntries(ShardedStateBus* bus, size_t* count)
{
    if (!bus || !count)
        return nullptr;

    *count = 0;
    std::vector<StateEntry> all_entries;

    for (size_t i = 0; i < bus->num_shards; i++)
    {
        size_t      shard_count = 0;
        StateEntry* entries     = StateBus_GetAllEntries(bus->shards[i], &shard_count);
        if (entries && shard_count > 0)
        {
            for (size_t j = 0; j < shard_count; j++)
            {
                all_entries.push_back(entries[j]);
            }
            StateBus_FreeEntries(entries, shard_count);
        }
    }

    *count = all_entries.size();
    if (all_entries.empty())
        return nullptr;

    StateEntry* result = (StateEntry*)malloc(all_entries.size() * sizeof(StateEntry));
    if (!result)
        return nullptr;

    for (size_t i = 0; i < all_entries.size(); i++)
    {
        const StateEntry& src = all_entries[i];
        result[i].path        = strdup(src.path);
        result[i].state       = src.state;
        result[i].version     = src.version;
        result[i].timestamp   = src.timestamp;
        result[i].dataSize    = src.dataSize;
        if (src.dataSnapshot && src.dataSize > 0)
        {
            result[i].dataSnapshot = malloc(src.dataSize);
            if (result[i].dataSnapshot)
            {
                memcpy((void*)result[i].dataSnapshot, src.dataSnapshot, src.dataSize);
            }
        }
        else
        {
            result[i].dataSnapshot = nullptr;
        }
        result[i].next = nullptr;
    }

    return result;
}

void ShardedStateBus_FreeEntries(StateEntry* entries, size_t count)
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

uint64_t ShardedStateBus_GetTotalOperations(ShardedStateBus* bus)
{
    if (!bus)
        return 0;
    return bus->total_operations.load();
}

size_t ShardedStateBus_GetShardCount(ShardedStateBus* bus)
{
    if (!bus)
        return 0;
    return bus->num_shards;
}
