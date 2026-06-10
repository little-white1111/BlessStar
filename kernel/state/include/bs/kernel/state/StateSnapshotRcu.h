#ifndef BS_KERNEL_STATE_STATESNAPSHOTRCU_H
#define BS_KERNEL_STATE_STATESNAPSHOTRCU_H

/*
 * Seqlock + immutable payload (pointer RCU) for StateBus snapshot reads.
 * Writers publish new shared_ptr; readers pin without holding StateBus mutex.
 */

#include "bs/kernel/state/ConfigState.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

struct StateBus;

struct BsStateSnapshotPayload
{
    std::vector<uint8_t> bytes;
    ConfigState          state = CONFIG_STATE_INITIAL;
    uint64_t             version = 0;
};

/** Seqlock read; returns null shared_ptr if path is missing. */
std::shared_ptr<const BsStateSnapshotPayload> bs_state_bus_pin_snapshot(StateBus* bus,
                                                                          const char* path);

#endif /* BS_KERNEL_STATE_STATESNAPSHOTRCU_H */
