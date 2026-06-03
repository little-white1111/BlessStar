#ifndef BS_KERNEL_STATE_STATEMACHINE_H
#define BS_KERNEL_STATE_STATEMACHINE_H

/*
 * C-ST-7 contract block:
 * Thread safety: Not thread-safe; one StateMachine per controlling thread.
 * Error semantics: Transition failures return non-zero; illegal transitions rejected.
 * Platform notes: C++ implementation; complements StateBus path-level state.
 */

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

    StateMachine* bs_state_machine_create(const char* configPath);

    void bs_state_machine_destroy(StateMachine* sm);

    ConfigState bs_state_machine_get_current_state(const StateMachine* sm);

    int bs_state_machine_transition(StateMachine* sm, ConfigState newState);

    int bs_state_machine_can_transition(const StateMachine* sm, ConfigState newState);

    void bs_state_machine_set_callback(StateMachine* sm, StateTransitionCallback callback);

    uint64_t bs_state_machine_get_version(const StateMachine* sm);

#ifdef __cplusplus
}
#endif

#endif
