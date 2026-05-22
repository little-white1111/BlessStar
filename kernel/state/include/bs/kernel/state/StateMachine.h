#ifndef BS_KERNEL_STATE_STATEMACHINE_H
#define BS_KERNEL_STATE_STATEMACHINE_H

#include "ConfigState.h"

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#else
#include <stddef.h>
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct StateMachine StateMachine;

    typedef void (*StateTransitionCallback)(const char* configPath, ConfigState from,
                                            ConfigState to);

    StateMachine* StateMachine_Create(const char* configPath);

    void StateMachine_Destroy(StateMachine* sm);

    ConfigState StateMachine_GetCurrentState(const StateMachine* sm);

    int StateMachine_Transition(StateMachine* sm, ConfigState newState);

    int StateMachine_CanTransition(const StateMachine* sm, ConfigState newState);

    void StateMachine_SetCallback(StateMachine* sm, StateTransitionCallback callback);

    uint64_t StateMachine_GetVersion(const StateMachine* sm);

#ifdef __cplusplus
}
#endif

#endif
