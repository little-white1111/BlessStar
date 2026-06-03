#include "bs/kernel/state/EventBus.h"
#include "bs/kernel/state/StateBus.h"
#include "bs/kernel/state/StateMachine.h"
#include "bs/kernel/state/TemporaryState.h"
#include "bs/kernel/state/WatchManager.h"

#include <cassert>
#include <cstdio>
#include <cstring>

static void test_StateMachine_NullInput()
{
    StateMachine* sm = bs_state_machine_create(nullptr);
    assert(sm != nullptr);
    bs_state_machine_destroy(sm);
    printf("test_StateMachine_NullInput: PASS\n");
}

static void test_StateMachine_EmptyName()
{
    StateMachine* sm = bs_state_machine_create("");
    assert(sm != nullptr);
    bs_state_machine_destroy(sm);
    printf("test_StateMachine_EmptyName: PASS\n");
}

static void test_StateMachine_InvalidTransition()
{
    StateMachine* sm = bs_state_machine_create("test");

    int result = bs_state_machine_transition(sm, static_cast<ConfigState>(999));
    assert(result == -2);

    bs_state_machine_destroy(sm);
    printf("test_StateMachine_InvalidTransition: PASS\n");
}

static void test_StateBus_NullInput()
{
    StateBus* bus = bs_state_bus_create();
    bs_state_bus_set_state(bus, nullptr, CONFIG_STATE_INITIAL, nullptr, 0);
    StateEntry* entry = nullptr;
    bs_state_bus_get_state(bus, nullptr, &entry);
    bs_state_bus_destroy(bus);
    printf("test_StateBus_NullInput: PASS\n");
}

static void test_StateBus_EmptyPath()
{
    StateBus*   bus   = bs_state_bus_create();
    StateEntry* entry = nullptr;
    bs_state_bus_get_state(bus, "", &entry);
    assert(entry == nullptr);
    bs_state_bus_destroy(bus);
    printf("test_StateBus_EmptyPath: PASS\n");
}

static void test_StateBus_LongPath()
{
    char long_path[1024];
    memset(long_path, 'a', sizeof(long_path) - 1);
    long_path[sizeof(long_path) - 1] = '\0';

    StateBus*  bus    = bs_state_bus_create();
    const char data[] = "test";
    bs_state_bus_set_state(bus, long_path, CONFIG_STATE_ACTIVE, data, sizeof(data));
    StateEntry* entry = nullptr;
    bs_state_bus_get_state(bus, long_path, &entry);
    assert(entry != nullptr);
    bs_state_bus_destroy(bus);
    printf("test_StateBus_LongPath: PASS\n");
}

static void test_StateBus_LargeEntryCount()
{
    StateBus* bus = bs_state_bus_create();
    const int N   = 10000;

    for (int i = 0; i < N; i++)
    {
        char path[64];
        sprintf(path, "/test/%d", i);
        const char data[] = "value";
        bs_state_bus_set_state(bus, path, CONFIG_STATE_ACTIVE, data, sizeof(data));
    }

    bs_state_bus_destroy(bus);
    printf("test_StateBus_LargeEntryCount: PASS\n");
}

static void test_EventBus_NullInput()
{
    EventBus* bus = bs_event_bus_create();
    bs_event_bus_subscribe(bus, nullptr, nullptr, nullptr);
    bs_event_bus_unsubscribe(bus, nullptr, nullptr);
    ConfigEvent* event = bs_config_event_create("", CONFIG_EVENT_ENTER_INITIAL,
                                                CONFIG_STATE_INITIAL, CONFIG_STATE_LOADING, 1);
    bs_event_bus_publish(bus, event);
    bs_config_event_destroy(event);
    bs_event_bus_drain(bus);
    bs_event_bus_destroy(bus);
    printf("test_EventBus_NullInput: PASS\n");
}

static void test_WatchManager_NullInput()
{
    WatchManager* wm = bs_watch_manager_create();
    bs_watch_manager_add_watch(wm, nullptr, nullptr, WATCH_MODE_ONCE, nullptr);
    bs_watch_manager_remove_watch(wm, nullptr, nullptr);
    bs_watch_manager_notify(wm, "", CONFIG_EVENT_ENTER_INITIAL, nullptr);
    bs_watch_manager_destroy(wm);
    printf("test_WatchManager_NullInput: PASS\n");
}

static void test_TemporaryState_NullInput()
{
    TemporaryState* temp = bs_temporary_state_create(nullptr, nullptr, 0);
    assert(temp == nullptr);
    printf("test_TemporaryState_NullInput: PASS\n");
}

int main()
{
    printf("=== State Boundary Tests ===\n");
    test_StateMachine_NullInput();
    test_StateMachine_EmptyName();
    test_StateMachine_InvalidTransition();
    test_StateBus_NullInput();
    test_StateBus_EmptyPath();
    test_StateBus_LongPath();
    test_StateBus_LargeEntryCount();
    test_EventBus_NullInput();
    test_WatchManager_NullInput();
    test_TemporaryState_NullInput();
    printf("=== All State Boundary Tests PASSED! ===\n");
    return 0;
}
