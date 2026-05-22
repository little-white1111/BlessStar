#ifndef BS_KERNEL_STATE_SHARDED_STATE_BUS_H
#define BS_KERNEL_STATE_SHARDED_STATE_BUS_H

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

    ShardedStateBus* ShardedStateBus_Create(size_t num_shards);

    void ShardedStateBus_Destroy(ShardedStateBus* bus);

    int ShardedStateBus_SetState(ShardedStateBus* bus, const char* path, ConfigState state,
                                 const void* data, size_t dataSize);

    int ShardedStateBus_GetState(ShardedStateBus* bus, const char* path, StateEntry** entry);

    int ShardedStateBus_GetSnapshot(ShardedStateBus* bus, const char* path, void** data,
                                    size_t* size);

    StateEntry* ShardedStateBus_GetAllEntries(ShardedStateBus* bus, size_t* count);

    void ShardedStateBus_FreeEntries(StateEntry* entries, size_t count);

    uint64_t ShardedStateBus_GetTotalOperations(ShardedStateBus* bus);

    size_t ShardedStateBus_GetShardCount(ShardedStateBus* bus);

#ifdef __cplusplus
}
#endif

#endif
