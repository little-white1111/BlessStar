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
    StateMachine* sm = bs_state_machine_create("test/config");
    assert(sm != nullptr);
    assert(bs_state_machine_get_current_state(sm) == CONFIG_STATE_INITIAL);
    assert(bs_state_machine_get_version(sm) == 0);
    bs_state_machine_destroy(sm);
    printf("bs_state_machine_createDestroy: PASS\n");

    // Test Valid Transitions
    sm = bs_state_machine_create("test/config");
    assert(bs_state_machine_transition(sm, CONFIG_STATE_LOADING) == 0);
    assert(bs_state_machine_transition(sm, CONFIG_STATE_ACTIVE) == 0);
    assert(bs_state_machine_transition(sm, CONFIG_STATE_UPDATING) == 0);
    assert(bs_state_machine_transition(sm, CONFIG_STATE_ACTIVE) == 0);
    assert(bs_state_machine_transition(sm, CONFIG_STATE_CLOSED) == 0);
    bs_state_machine_destroy(sm);
    printf("StateMachine_ValidTransitions: PASS\n");

    // Test Invalid Transitions
    sm = bs_state_machine_create("test/config");
    assert(bs_state_machine_transition(sm, CONFIG_STATE_ACTIVE) == -2);
    bs_state_machine_destroy(sm);
    printf("StateMachine_InvalidTransitions: PASS\n");

    printf("\n--- StateBus Tests ---\n");

    // Test Create/Destroy
    StateBus* bus = bs_state_bus_create();
    assert(bus != nullptr);
    bs_state_bus_destroy(bus);
    printf("bs_state_bus_createDestroy: PASS\n");

    // Test Set/Get State
    bus              = bs_state_bus_create();
    const char* data = "test data";
    assert(bs_state_bus_set_state(bus, "/config/service1", CONFIG_STATE_ACTIVE, data,
                                  strlen(data) + 1) == 0);
    StateEntry* entry = nullptr;
    assert(bs_state_bus_get_state(bus, "/config/service1", &entry) == 0);
    assert(entry != nullptr);
    assert(entry->state == CONFIG_STATE_ACTIVE);
    bs_state_bus_destroy(bus);
    printf("StateBus_SetGetState: PASS\n");

    // Test Null Input
    bus = bs_state_bus_create();
    assert(bs_state_bus_set_state(nullptr, "/config/test", CONFIG_STATE_ACTIVE, nullptr, 0) == -1);
    assert(bs_state_bus_set_state(bus, nullptr, CONFIG_STATE_ACTIVE, nullptr, 0) == -1);
    bs_state_bus_destroy(bus);
    printf("StateBus_NullInput: PASS\n");

    printf("\n=== All Kernel State Tests PASSED! ===\n");
    return 0;
}
