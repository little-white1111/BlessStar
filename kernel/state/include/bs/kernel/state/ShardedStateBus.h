#ifndef BS_KERNEL_STATE_SHARDED_STATE_BUS_H
#define BS_KERNEL_STATE_SHARDED_STATE_BUS_H

/*
 * C-ST-7 contract block:
 * Thread safety: Shard-level locking; route by path hash.
 * Error semantics: Same as StateBus per shard; aggregate helpers may return partial data.
 * Platform notes: Wraps multiple StateBus instances for contention reduction.
 */

#include "ConfigState.h"
#include "StateBus.h"

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
extern "C"
{
#else
#include <stddef.h>
#include <stdint.h>
#endif

    typedef struct ShardedStateBus ShardedStateBus;

    ShardedStateBus* bs_sharded_state_bus_create(size_t num_shards);

    void bs_sharded_state_bus_destroy(ShardedStateBus* bus);

    int bs_sharded_state_bus_set_state(ShardedStateBus* bus, const char* path, ConfigState state,
                                       const void* data, size_t dataSize);

    int bs_sharded_state_bus_get_state(ShardedStateBus* bus, const char* path, StateEntry** entry);

    int bs_sharded_state_bus_get_snapshot(ShardedStateBus* bus, const char* path, void** data,
                                          size_t* size);

    StateEntry* bs_sharded_state_bus_get_all_entries(ShardedStateBus* bus, size_t* count);

    void bs_sharded_state_bus_free_entries(StateEntry* entries, size_t count);

    uint64_t bs_sharded_state_bus_get_total_operations(ShardedStateBus* bus);

    size_t bs_sharded_state_bus_get_shard_count(ShardedStateBus* bus);

#ifdef __cplusplus
}
#endif

#endif
