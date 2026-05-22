#ifndef BS_KERNEL_STATE_STATEBUS_H
#define BS_KERNEL_STATE_STATEBUS_H

#include "ConfigState.h"

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#else
#include <stddef.h>
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct StateEntry
    {
        const char*        path;
        ConfigState        state;
        uint64_t           version;
        uint64_t           timestamp;
        const void*        dataSnapshot;
        size_t             dataSize;
        struct StateEntry* next;
    } StateEntry;

    typedef struct StateBus StateBus;

    StateBus* StateBus_Create();

    void StateBus_Destroy(StateBus* bus);

    int StateBus_SetState(StateBus* bus, const char* path, ConfigState state, const void* data,
                          size_t dataSize);

    int StateBus_GetState(StateBus* bus, const char* path, StateEntry** entry);

    int StateBus_GetSnapshot(StateBus* bus, const char* path, void** data, size_t* size);

    StateEntry* StateBus_GetAllEntries(StateBus* bus, size_t* count);

    void StateBus_FreeEntries(StateEntry* entries, size_t count);

#ifdef __cplusplus
}
#endif

#endif
