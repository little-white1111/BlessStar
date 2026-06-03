#include "bs/kernel/state/ShardedStateBus.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void test_bs_sharded_state_bus_createDestroy()
{
    ShardedStateBus* bus = bs_sharded_state_bus_create(16);
    assert(bus != nullptr);
    assert(bs_sharded_state_bus_get_shard_count(bus) == 16);
    bs_sharded_state_bus_destroy(bus);
    printf("test_bs_sharded_state_bus_createDestroy: PASS\n");
}

static void test_ShardedStateBus_SetGetState()
{
    ShardedStateBus* bus  = bs_sharded_state_bus_create(16);
    const char*      data = "test data";

    int result = bs_sharded_state_bus_set_state(bus, "/config/service1", CONFIG_STATE_ACTIVE, data,
                                                strlen(data) + 1);
    assert(result == 0);

    StateEntry* entry = nullptr;
    result            = bs_sharded_state_bus_get_state(bus, "/config/service1", &entry);
    assert(result == 0);
    assert(entry != nullptr);
    assert(entry->state == CONFIG_STATE_ACTIVE);
    assert(entry->version == 1);

    bs_sharded_state_bus_destroy(bus);
    printf("test_ShardedStateBus_SetGetState: PASS\n");
}

static void test_ShardedStateBus_MultiplePaths()
{
    ShardedStateBus* bus = bs_sharded_state_bus_create(4);

    for (int i = 0; i < 100; i++)
    {
        char path[64];
        sprintf(path, "/config/service%d", i);
        int result =
            bs_sharded_state_bus_set_state(bus, path, CONFIG_STATE_ACTIVE, path, strlen(path) + 1);
        assert(result == 0);
    }

    assert(bs_sharded_state_bus_get_total_operations(bus) == 100);

    for (int i = 0; i < 100; i++)
    {
        char path[64];
        sprintf(path, "/config/service%d", i);
        StateEntry* entry  = nullptr;
        int         result = bs_sharded_state_bus_get_state(bus, path, &entry);
        assert(result == 0);
        assert(entry != nullptr);
    }

    bs_sharded_state_bus_destroy(bus);
    printf("test_ShardedStateBus_MultiplePaths: PASS\n");
}

static void test_bs_sharded_state_bus_get_snapshot()
{
    ShardedStateBus* bus  = bs_sharded_state_bus_create(16);
    const char*      data = "snapshot test";

    bs_sharded_state_bus_set_state(bus, "/config/test", CONFIG_STATE_ACTIVE, data,
                                   strlen(data) + 1);

    void*  snapshot = nullptr;
    size_t size     = 0;
    int    result   = bs_sharded_state_bus_get_snapshot(bus, "/config/test", &snapshot, &size);
    assert(result == 0);
    assert(snapshot != nullptr);
    assert(size == strlen(data) + 1);
    assert(memcmp(snapshot, data, size) == 0);

    free(snapshot);
    bs_sharded_state_bus_destroy(bus);
    printf("test_bs_sharded_state_bus_get_snapshot: PASS\n");
}

static void test_bs_sharded_state_bus_get_all_entries()
{
    ShardedStateBus* bus = bs_sharded_state_bus_create(4);

    bs_sharded_state_bus_set_state(bus, "/config/a", CONFIG_STATE_ACTIVE, "a", 2);
    bs_sharded_state_bus_set_state(bus, "/config/b", CONFIG_STATE_LOADING, "b", 2);
    bs_sharded_state_bus_set_state(bus, "/config/c", CONFIG_STATE_ERROR, "c", 2);

    size_t      count   = 0;
    StateEntry* entries = bs_sharded_state_bus_get_all_entries(bus, &count);
    assert(count == 3);
    assert(entries != nullptr);

    bs_sharded_state_bus_free_entries(entries, count);
    bs_sharded_state_bus_destroy(bus);
    printf("test_bs_sharded_state_bus_get_all_entries: PASS\n");
}

static void test_ShardedStateBus_NullInput()
{
    ShardedStateBus* bus = bs_sharded_state_bus_create(16);

    int result =
        bs_sharded_state_bus_set_state(nullptr, "/config/test", CONFIG_STATE_ACTIVE, nullptr, 0);
    assert(result == -1);

    result = bs_sharded_state_bus_set_state(bus, nullptr, CONFIG_STATE_ACTIVE, nullptr, 0);
    assert(result == -1);

    StateEntry* entry = nullptr;
    result            = bs_sharded_state_bus_get_state(nullptr, "/config/test", &entry);
    assert(result == -1);

    assert(bs_sharded_state_bus_get_total_operations(nullptr) == 0);
    assert(bs_sharded_state_bus_get_shard_count(nullptr) == 0);

    bs_sharded_state_bus_destroy(bus);
    printf("test_ShardedStateBus_NullInput: PASS\n");
}

int main()
{
    printf("=== ShardedStateBus Tests ===\n");
    test_bs_sharded_state_bus_createDestroy();
    test_ShardedStateBus_SetGetState();
    test_ShardedStateBus_MultiplePaths();
    test_bs_sharded_state_bus_get_snapshot();
    test_bs_sharded_state_bus_get_all_entries();
    test_ShardedStateBus_NullInput();
    printf("=== All ShardedStateBus Tests PASSED! ===\n");
    return 0;
}
