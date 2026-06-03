#include "bs/kernel/state/StateBus.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void test_bs_state_bus_createDestroy()
{
    StateBus* bus = bs_state_bus_create();
    assert(bus != nullptr);

    StateEntry* entry = nullptr;
    int         ret   = bs_state_bus_get_state(bus, "test/config", &entry);
    assert(ret == -2);
    assert(entry == nullptr);

    bs_state_bus_destroy(bus);
    printf("test_bs_state_bus_createDestroy: PASS\n");
}

static void test_StateBus_SetGetState()
{
    StateBus* bus = bs_state_bus_create();
    assert(bus != nullptr);

    char data[] = "test data";
    bs_state_bus_set_state(bus, "test/config", CONFIG_STATE_ACTIVE, data, sizeof(data));

    StateEntry* entry = nullptr;
    int         ret   = bs_state_bus_get_state(bus, "test/config", &entry);
    assert(ret == 0);
    assert(entry != nullptr);
    assert(strcmp(entry->path, "test/config") == 0);
    assert(entry->state == CONFIG_STATE_ACTIVE);
    assert(entry->version == 1);
    assert(entry->dataSize == sizeof(data));
    assert(memcmp(entry->dataSnapshot, data, sizeof(data)) == 0);

    bs_state_bus_destroy(bus);
    printf("test_StateBus_SetGetState: PASS\n");
}

static void test_StateBus_VersionIncrement()
{
    StateBus* bus = bs_state_bus_create();
    assert(bus != nullptr);

    char data1[] = "data1";
    bs_state_bus_set_state(bus, "test/config", CONFIG_STATE_LOADING, data1, sizeof(data1));

    StateEntry* entry = nullptr;
    bs_state_bus_get_state(bus, "test/config", &entry);
    assert(entry->version == 1);

    char data2[] = "data2";
    bs_state_bus_set_state(bus, "test/config", CONFIG_STATE_ACTIVE, data2, sizeof(data2));

    bs_state_bus_get_state(bus, "test/config", &entry);
    assert(entry->version == 2);

    bs_state_bus_destroy(bus);
    printf("test_StateBus_VersionIncrement: PASS\n");
}

static void test_bs_state_bus_get_snapshot()
{
    StateBus* bus = bs_state_bus_create();
    assert(bus != nullptr);

    char data[] = "snapshot data";
    bs_state_bus_set_state(bus, "test/config", CONFIG_STATE_ACTIVE, data, sizeof(data));

    void*  snapshot = nullptr;
    size_t size     = 0;
    int    ret      = bs_state_bus_get_snapshot(bus, "test/config", &snapshot, &size);
    assert(ret == 0);
    assert(snapshot != nullptr);
    assert(size == sizeof(data));
    assert(memcmp(snapshot, data, size) == 0);

    free(snapshot);
    bs_state_bus_destroy(bus);
    printf("test_bs_state_bus_get_snapshot: PASS\n");
}

static void test_bs_state_bus_get_all_entries()
{
    StateBus* bus = bs_state_bus_create();
    assert(bus != nullptr);

    char data1[] = "config1 data";
    char data2[] = "config2 data";
    char data3[] = "config3 data";

    bs_state_bus_set_state(bus, "test/config1", CONFIG_STATE_ACTIVE, data1, sizeof(data1));
    bs_state_bus_set_state(bus, "test/config2", CONFIG_STATE_LOADING, data2, sizeof(data2));
    bs_state_bus_set_state(bus, "test/config3", CONFIG_STATE_ERROR, data3, sizeof(data3));

    size_t      count   = 0;
    StateEntry* entries = bs_state_bus_get_all_entries(bus, &count);
    assert(entries != nullptr);
    assert(count == 3);

    bool found1 = false, found2 = false, found3 = false;
    for (size_t i = 0; i < count; i++)
    {
        if (strcmp(entries[i].path, "test/config1") == 0)
        {
            assert(entries[i].state == CONFIG_STATE_ACTIVE);
            found1 = true;
        }
        if (strcmp(entries[i].path, "test/config2") == 0)
        {
            assert(entries[i].state == CONFIG_STATE_LOADING);
            found2 = true;
        }
        if (strcmp(entries[i].path, "test/config3") == 0)
        {
            assert(entries[i].state == CONFIG_STATE_ERROR);
            found3 = true;
        }
    }
    assert(found1 && found2 && found3);

    bs_state_bus_free_entries(entries, count);
    bs_state_bus_destroy(bus);
    printf("test_bs_state_bus_get_all_entries: PASS\n");
}

static void test_StateBus_NullData()
{
    StateBus* bus = bs_state_bus_create();
    assert(bus != nullptr);

    bs_state_bus_set_state(bus, "test/config", CONFIG_STATE_ACTIVE, nullptr, 0);

    StateEntry* entry = nullptr;
    bs_state_bus_get_state(bus, "test/config", &entry);
    assert(entry != nullptr);
    assert(entry->state == CONFIG_STATE_ACTIVE);
    assert(entry->dataSnapshot == nullptr);
    assert(entry->dataSize == 0);

    bs_state_bus_destroy(bus);
    printf("test_StateBus_NullData: PASS\n");
}

int main()
{
    printf("=== StateBus Tests ===\n");
    test_bs_state_bus_createDestroy();
    test_StateBus_SetGetState();
    test_StateBus_VersionIncrement();
    test_bs_state_bus_get_snapshot();
    test_bs_state_bus_get_all_entries();
    test_StateBus_NullData();
    printf("=== All StateBus Tests PASSED! ===\n");
    return 0;
}
