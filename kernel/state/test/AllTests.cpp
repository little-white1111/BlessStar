#include "bs/kernel/state/ConfigState.h"
#include "bs/kernel/state/StateBus.h"
#include "bs/kernel/state/StateMachine.h"

#include <cassert>
#include <cstdio>
#include <cstring>

int main()
{
    printf("=== Running All Kernel State Tests ===\n\n");

    printf("--- StateMachine Tests ---\n");

    // Test Create/Destroy
    StateMachine* sm = StateMachine_Create("test/config");
    assert(sm != nullptr);
    assert(StateMachine_GetCurrentState(sm) == CONFIG_STATE_INITIAL);
    assert(StateMachine_GetVersion(sm) == 0);
    StateMachine_Destroy(sm);
    printf("StateMachine_CreateDestroy: PASS\n");

    // Test Valid Transitions
    sm = StateMachine_Create("test/config");
    assert(StateMachine_Transition(sm, CONFIG_STATE_LOADING) == 0);
    assert(StateMachine_Transition(sm, CONFIG_STATE_ACTIVE) == 0);
    assert(StateMachine_Transition(sm, CONFIG_STATE_UPDATING) == 0);
    assert(StateMachine_Transition(sm, CONFIG_STATE_ACTIVE) == 0);
    assert(StateMachine_Transition(sm, CONFIG_STATE_CLOSED) == 0);
    StateMachine_Destroy(sm);
    printf("StateMachine_ValidTransitions: PASS\n");

    // Test Invalid Transitions
    sm = StateMachine_Create("test/config");
    assert(StateMachine_Transition(sm, CONFIG_STATE_ACTIVE) == -2);
    StateMachine_Destroy(sm);
    printf("StateMachine_InvalidTransitions: PASS\n");

    printf("\n--- StateBus Tests ---\n");

    // Test Create/Destroy
    StateBus* bus = StateBus_Create();
    assert(bus != nullptr);
    StateBus_Destroy(bus);
    printf("StateBus_CreateDestroy: PASS\n");

    // Test Set/Get State
    bus              = StateBus_Create();
    const char* data = "test data";
    assert(StateBus_SetState(bus, "/config/service1", CONFIG_STATE_ACTIVE, data,
                             strlen(data) + 1) == 0);
    StateEntry* entry = nullptr;
    assert(StateBus_GetState(bus, "/config/service1", &entry) == 0);
    assert(entry != nullptr);
    assert(entry->state == CONFIG_STATE_ACTIVE);
    StateBus_Destroy(bus);
    printf("StateBus_SetGetState: PASS\n");

    // Test Null Input
    bus = StateBus_Create();
    assert(StateBus_SetState(nullptr, "/config/test", CONFIG_STATE_ACTIVE, nullptr, 0) == -1);
    assert(StateBus_SetState(bus, nullptr, CONFIG_STATE_ACTIVE, nullptr, 0) == -1);
    StateBus_Destroy(bus);
    printf("StateBus_NullInput: PASS\n");

    printf("\n=== All Kernel State Tests PASSED! ===\n");
    return 0;
}
