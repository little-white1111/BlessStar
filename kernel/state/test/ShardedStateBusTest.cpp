#include "bs/kernel/state/ShardedStateBus.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void test_ShardedStateBus_CreateDestroy()
{
    ShardedStateBus* bus = ShardedStateBus_Create(16);
    assert(bus != nullptr);
    assert(ShardedStateBus_GetShardCount(bus) == 16);
    ShardedStateBus_Destroy(bus);
    printf("test_ShardedStateBus_CreateDestroy: PASS\n");
}

static void test_ShardedStateBus_SetGetState()
{
    ShardedStateBus* bus  = ShardedStateBus_Create(16);
    const char*      data = "test data";

    int result = ShardedStateBus_SetState(bus, "/config/service1", CONFIG_STATE_ACTIVE, data,
                                          strlen(data) + 1);
    assert(result == 0);

    StateEntry* entry = nullptr;
    result            = ShardedStateBus_GetState(bus, "/config/service1", &entry);
    assert(result == 0);
    assert(entry != nullptr);
    assert(entry->state == CONFIG_STATE_ACTIVE);
    assert(entry->version == 1);

    ShardedStateBus_Destroy(bus);
    printf("test_ShardedStateBus_SetGetState: PASS\n");
}

static void test_ShardedStateBus_MultiplePaths()
{
    ShardedStateBus* bus = ShardedStateBus_Create(4);

    for (int i = 0; i < 100; i++)
    {
        char path[64];
        sprintf(path, "/config/service%d", i);
        int result =
            ShardedStateBus_SetState(bus, path, CONFIG_STATE_ACTIVE, path, strlen(path) + 1);
        assert(result == 0);
    }

    assert(ShardedStateBus_GetTotalOperations(bus) == 100);

    for (int i = 0; i < 100; i++)
    {
        char path[64];
        sprintf(path, "/config/service%d", i);
        StateEntry* entry  = nullptr;
        int         result = ShardedStateBus_GetState(bus, path, &entry);
        assert(result == 0);
        assert(entry != nullptr);
    }

    ShardedStateBus_Destroy(bus);
    printf("test_ShardedStateBus_MultiplePaths: PASS\n");
}

static void test_ShardedStateBus_GetSnapshot()
{
    ShardedStateBus* bus  = ShardedStateBus_Create(16);
    const char*      data = "snapshot test";

    ShardedStateBus_SetState(bus, "/config/test", CONFIG_STATE_ACTIVE, data, strlen(data) + 1);

    void*  snapshot = nullptr;
    size_t size     = 0;
    int    result   = ShardedStateBus_GetSnapshot(bus, "/config/test", &snapshot, &size);
    assert(result == 0);
    assert(snapshot != nullptr);
    assert(size == strlen(data) + 1);
    assert(memcmp(snapshot, data, size) == 0);

    free(snapshot);
    ShardedStateBus_Destroy(bus);
    printf("test_ShardedStateBus_GetSnapshot: PASS\n");
}

static void test_ShardedStateBus_GetAllEntries()
{
    ShardedStateBus* bus = ShardedStateBus_Create(4);

    ShardedStateBus_SetState(bus, "/config/a", CONFIG_STATE_ACTIVE, "a", 2);
    ShardedStateBus_SetState(bus, "/config/b", CONFIG_STATE_LOADING, "b", 2);
    ShardedStateBus_SetState(bus, "/config/c", CONFIG_STATE_ERROR, "c", 2);

    size_t      count   = 0;
    StateEntry* entries = ShardedStateBus_GetAllEntries(bus, &count);
    assert(count == 3);
    assert(entries != nullptr);

    ShardedStateBus_FreeEntries(entries, count);
    ShardedStateBus_Destroy(bus);
    printf("test_ShardedStateBus_GetAllEntries: PASS\n");
}

static void test_ShardedStateBus_NullInput()
{
    ShardedStateBus* bus = ShardedStateBus_Create(16);

    int result = ShardedStateBus_SetState(nullptr, "/config/test", CONFIG_STATE_ACTIVE, nullptr, 0);
    assert(result == -1);

    result = ShardedStateBus_SetState(bus, nullptr, CONFIG_STATE_ACTIVE, nullptr, 0);
    assert(result == -1);

    StateEntry* entry = nullptr;
    result            = ShardedStateBus_GetState(nullptr, "/config/test", &entry);
    assert(result == -1);

    assert(ShardedStateBus_GetTotalOperations(nullptr) == 0);
    assert(ShardedStateBus_GetShardCount(nullptr) == 0);

    ShardedStateBus_Destroy(bus);
    printf("test_ShardedStateBus_NullInput: PASS\n");
}

int main()
{
    printf("=== ShardedStateBus Tests ===\n");
    test_ShardedStateBus_CreateDestroy();
    test_ShardedStateBus_SetGetState();
    test_ShardedStateBus_MultiplePaths();
    test_ShardedStateBus_GetSnapshot();
    test_ShardedStateBus_GetAllEntries();
    test_ShardedStateBus_NullInput();
    printf("=== All ShardedStateBus Tests PASSED! ===\n");
    return 0;
}
