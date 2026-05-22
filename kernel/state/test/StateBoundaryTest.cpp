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
    StateMachine* sm = StateMachine_Create(nullptr);
    assert(sm != nullptr);
    StateMachine_Destroy(sm);
    printf("test_StateMachine_NullInput: PASS\n");
}

static void test_StateMachine_EmptyName()
{
    StateMachine* sm = StateMachine_Create("");
    assert(sm != nullptr);
    StateMachine_Destroy(sm);
    printf("test_StateMachine_EmptyName: PASS\n");
}

static void test_StateMachine_InvalidTransition()
{
    StateMachine* sm = StateMachine_Create("test");

    int result = StateMachine_Transition(sm, static_cast<ConfigState>(999));
    assert(result == -2);

    StateMachine_Destroy(sm);
    printf("test_StateMachine_InvalidTransition: PASS\n");
}

static void test_StateBus_NullInput()
{
    StateBus* bus = StateBus_Create();
    StateBus_SetState(bus, nullptr, CONFIG_STATE_INITIAL, nullptr, 0);
    StateEntry* entry = nullptr;
    StateBus_GetState(bus, nullptr, &entry);
    StateBus_Destroy(bus);
    printf("test_StateBus_NullInput: PASS\n");
}

static void test_StateBus_EmptyPath()
{
    StateBus*   bus   = StateBus_Create();
    StateEntry* entry = nullptr;
    StateBus_GetState(bus, "", &entry);
    assert(entry == nullptr);
    StateBus_Destroy(bus);
    printf("test_StateBus_EmptyPath: PASS\n");
}

static void test_StateBus_LongPath()
{
    char long_path[1024];
    memset(long_path, 'a', sizeof(long_path) - 1);
    long_path[sizeof(long_path) - 1] = '\0';

    StateBus*  bus    = StateBus_Create();
    const char data[] = "test";
    StateBus_SetState(bus, long_path, CONFIG_STATE_ACTIVE, data, sizeof(data));
    StateEntry* entry = nullptr;
    StateBus_GetState(bus, long_path, &entry);
    assert(entry != nullptr);
    StateBus_Destroy(bus);
    printf("test_StateBus_LongPath: PASS\n");
}

static void test_StateBus_LargeEntryCount()
{
    StateBus* bus = StateBus_Create();
    const int N   = 10000;

    for (int i = 0; i < N; i++)
    {
        char path[64];
        sprintf(path, "/test/%d", i);
        const char data[] = "value";
        StateBus_SetState(bus, path, CONFIG_STATE_ACTIVE, data, sizeof(data));
    }

    StateBus_Destroy(bus);
    printf("test_StateBus_LargeEntryCount: PASS\n");
}

static void test_EventBus_NullInput()
{
    EventBus* bus = EventBus_Create();
    EventBus_Subscribe(bus, nullptr, nullptr, nullptr);
    EventBus_Unsubscribe(bus, nullptr, nullptr);
    ConfigEvent* event = ConfigEvent_Create("", CONFIG_EVENT_ENTER_INITIAL, CONFIG_STATE_INITIAL,
                                            CONFIG_STATE_LOADING, 1);
    EventBus_Publish(bus, event);
    ConfigEvent_Destroy(event);
    EventBus_Drain(bus);
    EventBus_Destroy(bus);
    printf("test_EventBus_NullInput: PASS\n");
}

static void test_WatchManager_NullInput()
{
    WatchManager* wm = WatchManager_Create();
    WatchManager_AddWatch(wm, nullptr, nullptr, WATCH_MODE_ONCE, nullptr);
    WatchManager_RemoveWatch(wm, nullptr, nullptr);
    WatchManager_Notify(wm, "", CONFIG_EVENT_ENTER_INITIAL, nullptr);
    WatchManager_Destroy(wm);
    printf("test_WatchManager_NullInput: PASS\n");
}

static void test_TemporaryState_NullInput()
{
    TemporaryState* temp = TemporaryState_Create(nullptr, nullptr, 0);
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
