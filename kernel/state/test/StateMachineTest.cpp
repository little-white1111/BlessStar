#include "bs/kernel/state/ConfigState.h"
#include "bs/kernel/state/StateMachine.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void test_bs_state_machine_createDestroy()
{
    StateMachine* sm = bs_state_machine_create("test/config");
    assert(sm != nullptr);

    assert(bs_state_machine_get_current_state(sm) == CONFIG_STATE_INITIAL);
    assert(bs_state_machine_get_version(sm) == 0);

    bs_state_machine_destroy(sm);
    printf("test_bs_state_machine_createDestroy: PASS\n");
}

static void test_StateMachine_ValidTransitions()
{
    StateMachine* sm = bs_state_machine_create("test/config");
    assert(sm != nullptr);

    assert(bs_state_machine_transition(sm, CONFIG_STATE_LOADING) == 0);
    assert(bs_state_machine_get_current_state(sm) == CONFIG_STATE_LOADING);
    assert(bs_state_machine_get_version(sm) == 1);

    assert(bs_state_machine_transition(sm, CONFIG_STATE_ACTIVE) == 0);
    assert(bs_state_machine_get_current_state(sm) == CONFIG_STATE_ACTIVE);
    assert(bs_state_machine_get_version(sm) == 2);

    assert(bs_state_machine_transition(sm, CONFIG_STATE_UPDATING) == 0);
    assert(bs_state_machine_get_current_state(sm) == CONFIG_STATE_UPDATING);
    assert(bs_state_machine_get_version(sm) == 3);

    assert(bs_state_machine_transition(sm, CONFIG_STATE_ACTIVE) == 0);
    assert(bs_state_machine_get_current_state(sm) == CONFIG_STATE_ACTIVE);
    assert(bs_state_machine_get_version(sm) == 4);

    assert(bs_state_machine_transition(sm, CONFIG_STATE_CLOSED) == 0);
    assert(bs_state_machine_get_current_state(sm) == CONFIG_STATE_CLOSED);
    assert(bs_state_machine_get_version(sm) == 5);

    bs_state_machine_destroy(sm);
    printf("test_StateMachine_ValidTransitions: PASS\n");
}

static void test_StateMachine_InvalidTransitions()
{
    StateMachine* sm = bs_state_machine_create("test/config");
    assert(sm != nullptr);

    assert(bs_state_machine_transition(sm, CONFIG_STATE_ACTIVE) == -2);
    assert(bs_state_machine_get_current_state(sm) == CONFIG_STATE_INITIAL);
    assert(bs_state_machine_get_version(sm) == 0);

    bs_state_machine_transition(sm, CONFIG_STATE_LOADING);
    bs_state_machine_transition(sm, CONFIG_STATE_ACTIVE);

    assert(bs_state_machine_transition(sm, CONFIG_STATE_LOADING) == -2);
    assert(bs_state_machine_get_current_state(sm) == CONFIG_STATE_ACTIVE);
    assert(bs_state_machine_get_version(sm) == 2);

    bs_state_machine_destroy(sm);
    printf("test_StateMachine_InvalidTransitions: PASS\n");
}

static void test_bs_state_machine_can_transition()
{
    StateMachine* sm = bs_state_machine_create("test/config");
    assert(sm != nullptr);

    assert(bs_state_machine_can_transition(sm, CONFIG_STATE_LOADING) == 1);
    assert(bs_state_machine_can_transition(sm, CONFIG_STATE_ACTIVE) == 0);
    assert(bs_state_machine_can_transition(sm, CONFIG_STATE_ERROR) == 0);

    bs_state_machine_transition(sm, CONFIG_STATE_LOADING);
    assert(bs_state_machine_can_transition(sm, CONFIG_STATE_ACTIVE) == 1);
    assert(bs_state_machine_can_transition(sm, CONFIG_STATE_ERROR) == 1);
    assert(bs_state_machine_can_transition(sm, CONFIG_STATE_CLOSED) == 0);

    bs_state_machine_destroy(sm);
    printf("test_bs_state_machine_can_transition: PASS\n");
}

struct StateMachineCallbackData
{
    int         calledCount;
    ConfigState lastFrom;
    ConfigState lastTo;
    const char* lastPath;
};

static StateMachineCallbackData g_callbackData = {};

static void stateMachineTransitionCallback(const char* path, ConfigState from, ConfigState to)
{
    g_callbackData.calledCount++;
    g_callbackData.lastFrom = from;
    g_callbackData.lastTo   = to;
    g_callbackData.lastPath = path;
}

static void test_StateMachine_Callback()
{
    StateMachine* sm = bs_state_machine_create("test/config");
    assert(sm != nullptr);

    g_callbackData = {};

    bs_state_machine_set_callback(sm, stateMachineTransitionCallback);

    bs_state_machine_transition(sm, CONFIG_STATE_LOADING);
    assert(g_callbackData.calledCount == 1);
    assert(g_callbackData.lastFrom == CONFIG_STATE_INITIAL);
    assert(g_callbackData.lastTo == CONFIG_STATE_LOADING);
    assert(strcmp(g_callbackData.lastPath, "test/config") == 0);

    bs_state_machine_destroy(sm);
    printf("test_StateMachine_Callback: PASS\n");
}

static void test_StateMachine_ToString()
{
    assert(strcmp(bs_config_state_to_string(CONFIG_STATE_INITIAL), "INITIAL") == 0);
    assert(strcmp(bs_config_state_to_string(CONFIG_STATE_LOADING), "LOADING") == 0);
    assert(strcmp(bs_config_state_to_string(CONFIG_STATE_ACTIVE), "ACTIVE") == 0);
    assert(strcmp(bs_config_state_to_string(CONFIG_STATE_UPDATING), "UPDATING") == 0);
    assert(strcmp(bs_config_state_to_string(CONFIG_STATE_ERROR), "ERROR") == 0);
    assert(strcmp(bs_config_state_to_string(CONFIG_STATE_CLOSED), "CLOSED") == 0);
    assert(strcmp(bs_config_state_to_string(static_cast<ConfigState>(999)), "UNKNOWN") == 0);

    printf("test_StateMachine_ToString: PASS\n");
}

int main()
{
    printf("=== StateMachine Tests ===\n");
    test_bs_state_machine_createDestroy();
    test_StateMachine_ValidTransitions();
    test_StateMachine_InvalidTransitions();
    test_bs_state_machine_can_transition();
    test_StateMachine_Callback();
    test_StateMachine_ToString();
    printf("=== All StateMachine Tests PASSED! ===\n");
    return 0;
}
