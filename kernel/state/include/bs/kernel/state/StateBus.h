#ifndef BS_KERNEL_STATE_STATEBUS_H
#define BS_KERNEL_STATE_STATEBUS_H

/*
 * C-ST-7 contract block:
 * Thread safety: Internally synchronized (shared_mutex); safe for concurrent readers.
 * Error semantics: 0 ok; -1 invalid; -2 missing path; -3 alloc failure on snapshot copy.
 * Platform notes: Owns path-keyed StateEntry map; snapshot reads use seqlock RCU pin (StateSnapshotRcu.h).
 */

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

    StateBus* bs_state_bus_create();

    void bs_state_bus_destroy(StateBus* bus);

    int bs_state_bus_set_state(StateBus* bus, const char* path, ConfigState state, const void* data,
                               size_t dataSize);

    int bs_state_bus_get_state(StateBus* bus, const char* path, StateEntry** entry);

    int bs_state_bus_get_snapshot(StateBus* bus, const char* path, void** data, size_t* size);

    StateEntry* bs_state_bus_get_all_entries(StateBus* bus, size_t* count);

    void bs_state_bus_free_entries(StateEntry* entries, size_t count);

#ifdef __cplusplus
}
#endif

#endif
