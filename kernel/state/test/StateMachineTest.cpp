#include "bs/kernel/state/ConfigState.h"
#include "bs/kernel/state/StateMachine.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void test_StateMachine_CreateDestroy()
{
    StateMachine* sm = StateMachine_Create("test/config");
    assert(sm != nullptr);

    assert(StateMachine_GetCurrentState(sm) == CONFIG_STATE_INITIAL);
    assert(StateMachine_GetVersion(sm) == 0);

    StateMachine_Destroy(sm);
    printf("test_StateMachine_CreateDestroy: PASS\n");
}

static void test_StateMachine_ValidTransitions()
{
    StateMachine* sm = StateMachine_Create("test/config");
    assert(sm != nullptr);

    assert(StateMachine_Transition(sm, CONFIG_STATE_LOADING) == 0);
    assert(StateMachine_GetCurrentState(sm) == CONFIG_STATE_LOADING);
    assert(StateMachine_GetVersion(sm) == 1);

    assert(StateMachine_Transition(sm, CONFIG_STATE_ACTIVE) == 0);
    assert(StateMachine_GetCurrentState(sm) == CONFIG_STATE_ACTIVE);
    assert(StateMachine_GetVersion(sm) == 2);

    assert(StateMachine_Transition(sm, CONFIG_STATE_UPDATING) == 0);
    assert(StateMachine_GetCurrentState(sm) == CONFIG_STATE_UPDATING);
    assert(StateMachine_GetVersion(sm) == 3);

    assert(StateMachine_Transition(sm, CONFIG_STATE_ACTIVE) == 0);
    assert(StateMachine_GetCurrentState(sm) == CONFIG_STATE_ACTIVE);
    assert(StateMachine_GetVersion(sm) == 4);

    assert(StateMachine_Transition(sm, CONFIG_STATE_CLOSED) == 0);
    assert(StateMachine_GetCurrentState(sm) == CONFIG_STATE_CLOSED);
    assert(StateMachine_GetVersion(sm) == 5);

    StateMachine_Destroy(sm);
    printf("test_StateMachine_ValidTransitions: PASS\n");
}

static void test_StateMachine_InvalidTransitions()
{
    StateMachine* sm = StateMachine_Create("test/config");
    assert(sm != nullptr);

    assert(StateMachine_Transition(sm, CONFIG_STATE_ACTIVE) == -2);
    assert(StateMachine_GetCurrentState(sm) == CONFIG_STATE_INITIAL);
    assert(StateMachine_GetVersion(sm) == 0);

    StateMachine_Transition(sm, CONFIG_STATE_LOADING);
    StateMachine_Transition(sm, CONFIG_STATE_ACTIVE);

    assert(StateMachine_Transition(sm, CONFIG_STATE_LOADING) == -2);
    assert(StateMachine_GetCurrentState(sm) == CONFIG_STATE_ACTIVE);
    assert(StateMachine_GetVersion(sm) == 2);

    StateMachine_Destroy(sm);
    printf("test_StateMachine_InvalidTransitions: PASS\n");
}

static void test_StateMachine_CanTransition()
{
    StateMachine* sm = StateMachine_Create("test/config");
    assert(sm != nullptr);

    assert(StateMachine_CanTransition(sm, CONFIG_STATE_LOADING) == 1);
    assert(StateMachine_CanTransition(sm, CONFIG_STATE_ACTIVE) == 0);
    assert(StateMachine_CanTransition(sm, CONFIG_STATE_ERROR) == 0);

    StateMachine_Transition(sm, CONFIG_STATE_LOADING);
    assert(StateMachine_CanTransition(sm, CONFIG_STATE_ACTIVE) == 1);
    assert(StateMachine_CanTransition(sm, CONFIG_STATE_ERROR) == 1);
    assert(StateMachine_CanTransition(sm, CONFIG_STATE_CLOSED) == 0);

    StateMachine_Destroy(sm);
    printf("test_StateMachine_CanTransition: PASS\n");
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
    StateMachine* sm = StateMachine_Create("test/config");
    assert(sm != nullptr);

    g_callbackData = {};

    StateMachine_SetCallback(sm, stateMachineTransitionCallback);

    StateMachine_Transition(sm, CONFIG_STATE_LOADING);
    assert(g_callbackData.calledCount == 1);
    assert(g_callbackData.lastFrom == CONFIG_STATE_INITIAL);
    assert(g_callbackData.lastTo == CONFIG_STATE_LOADING);
    assert(strcmp(g_callbackData.lastPath, "test/config") == 0);

    StateMachine_Destroy(sm);
    printf("test_StateMachine_Callback: PASS\n");
}

static void test_StateMachine_ToString()
{
    assert(strcmp(ConfigState_ToString(CONFIG_STATE_INITIAL), "INITIAL") == 0);
    assert(strcmp(ConfigState_ToString(CONFIG_STATE_LOADING), "LOADING") == 0);
    assert(strcmp(ConfigState_ToString(CONFIG_STATE_ACTIVE), "ACTIVE") == 0);
    assert(strcmp(ConfigState_ToString(CONFIG_STATE_UPDATING), "UPDATING") == 0);
    assert(strcmp(ConfigState_ToString(CONFIG_STATE_ERROR), "ERROR") == 0);
    assert(strcmp(ConfigState_ToString(CONFIG_STATE_CLOSED), "CLOSED") == 0);
    assert(strcmp(ConfigState_ToString(static_cast<ConfigState>(999)), "UNKNOWN") == 0);

    printf("test_StateMachine_ToString: PASS\n");
}

int main()
{
    printf("=== StateMachine Tests ===\n");
    test_StateMachine_CreateDestroy();
    test_StateMachine_ValidTransitions();
    test_StateMachine_InvalidTransitions();
    test_StateMachine_CanTransition();
    test_StateMachine_Callback();
    test_StateMachine_ToString();
    printf("=== All StateMachine Tests PASSED! ===\n");
    return 0;
}
