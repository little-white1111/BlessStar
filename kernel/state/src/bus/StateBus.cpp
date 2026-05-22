#include "bs/kernel/state/StateBus.h"

#include <cstdlib>
#include <cstring>
#include <ctime>

#include <shared_mutex>
#include <unordered_map>
#include <vector>

struct StateBus
{
    std::unordered_map<std::string, StateEntry*> entries;
    std::shared_mutex                            mutex;
};

StateBus* StateBus_Create()
{
    return new StateBus();
}

void StateBus_Destroy(StateBus* bus)
{
    if (!bus)
        return;
    for (auto& pair : bus->entries)
    {
        StateEntry* entry = pair.second;
        if (entry->dataSnapshot)
        {
            free((void*)entry->dataSnapshot);
        }
        delete entry;
    }
    delete bus;
}

int StateBus_SetState(StateBus* bus, const char* path, ConfigState state, const void* data,
                      size_t dataSize)
{
    if (!bus || !path)
        return -1;

    std::unique_lock<std::shared_mutex> lock(bus->mutex);

    auto        it    = bus->entries.find(path);
    StateEntry* entry = nullptr;

    if (it == bus->entries.end())
    {
        entry               = new StateEntry();
        entry->path         = strdup(path);
        entry->dataSnapshot = nullptr;
        entry->dataSize     = 0;
        entry->version      = 0;
        entry->next         = nullptr;
        bus->entries[path]  = entry;
    }
    else
    {
        entry = it->second;
    }

    entry->state = state;
    entry->version++;
    entry->timestamp = (uint64_t)time(nullptr);

    if (entry->dataSnapshot)
    {
        free((void*)entry->dataSnapshot);
    }
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

    return 0;
}

int StateBus_GetState(StateBus* bus, const char* path, StateEntry** entry)
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

    *entry = it->second;
    return 0;
}

int StateBus_GetSnapshot(StateBus* bus, const char* path, void** data, size_t* size)
{
    if (!bus || !path || !data || !size)
        return -1;

    std::shared_lock<std::shared_mutex> lock(bus->mutex);

    auto it = bus->entries.find(path);
    if (it == bus->entries.end())
        return -2;

    StateEntry* entry = it->second;
    if (!entry->dataSnapshot)
        return -2;

    *size = entry->dataSize;
    *data = malloc(entry->dataSize);
    if (!*data)
        return -3;
    memcpy(*data, entry->dataSnapshot, entry->dataSize);

    return 0;
}

StateEntry* StateBus_GetAllEntries(StateBus* bus, size_t* count)
{
    if (!bus || !count)
        return nullptr;

    std::shared_lock<std::shared_mutex> lock(bus->mutex);

    *count = bus->entries.size();
    if (bus->entries.empty())
        return nullptr;

    std::vector<StateEntry*> tempEntries;
    for (const auto& pair : bus->entries)
    {
        tempEntries.push_back(pair.second);
    }

    StateEntry* result = (StateEntry*)malloc(bus->entries.size() * sizeof(StateEntry));
    if (!result)
        return nullptr;

    for (size_t i = 0; i < bus->entries.size(); i++)
    {
        StateEntry* src     = tempEntries[i];
        result[i].path      = strdup(src->path);
        result[i].state     = src->state;
        result[i].version   = src->version;
        result[i].timestamp = src->timestamp;
        result[i].dataSize  = src->dataSize;
        if (src->dataSnapshot && src->dataSize > 0)
        {
            result[i].dataSnapshot = malloc(src->dataSize);
            if (result[i].dataSnapshot)
            {
                memcpy((void*)result[i].dataSnapshot, src->dataSnapshot, src->dataSize);
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

void StateBus_FreeEntries(StateEntry* entries, size_t count)
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
