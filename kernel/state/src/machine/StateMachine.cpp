#include "bs/kernel/state/ConfigEvent.h"
#include "bs/kernel/state/StateMachine.h"

#include <cstdlib>
#include <cstring>
#include <ctime>

#include <functional>
#include <string>
#include <vector>

struct StateTransition
{
    ConfigState from;
    ConfigState to;
};

struct StateMachine
{
    std::string                  configPath;
    ConfigState                  currentState;
    std::vector<StateTransition> transitions;
    StateTransitionCallback      callback;
    uint64_t                     version;
};

static const std::vector<StateTransition> defaultTransitions = {
    {CONFIG_STATE_INITIAL, CONFIG_STATE_LOADING}, {CONFIG_STATE_LOADING, CONFIG_STATE_ACTIVE},
    {CONFIG_STATE_LOADING, CONFIG_STATE_ERROR},   {CONFIG_STATE_ACTIVE, CONFIG_STATE_UPDATING},
    {CONFIG_STATE_ACTIVE, CONFIG_STATE_CLOSED},   {CONFIG_STATE_ACTIVE, CONFIG_STATE_ERROR},
    {CONFIG_STATE_UPDATING, CONFIG_STATE_ACTIVE}, {CONFIG_STATE_UPDATING, CONFIG_STATE_ERROR},
    {CONFIG_STATE_ERROR, CONFIG_STATE_INITIAL},   {CONFIG_STATE_ERROR, CONFIG_STATE_CLOSED}};

static const char* stateNames[] = {"INITIAL", "LOADING", "ACTIVE", "UPDATING", "ERROR", "CLOSED"};

#if defined(__clang__)
#define BS_NO_SANITIZE_ENUM __attribute__((no_sanitize("enum")))
#else
#define BS_NO_SANITIZE_ENUM
#endif

BS_NO_SANITIZE_ENUM const char* bs_config_state_to_string(ConfigState state)
{
    if (state < 0 || state >= sizeof(stateNames) / sizeof(stateNames[0]))
    {
        return "UNKNOWN";
    }
    return stateNames[state];
}

static BS_NO_SANITIZE_ENUM bool isValidTransition(ConfigState from, ConfigState to)
{
    for (const auto& trans : defaultTransitions)
    {
        if (trans.from == from && trans.to == to)
        {
            return true;
        }
    }
    return false;
}

StateMachine* bs_state_machine_create(const char* configPath)
{
    StateMachine* sm = new StateMachine();
    if (!sm)
        return nullptr;

    sm->configPath   = configPath ? configPath : "";
    sm->currentState = CONFIG_STATE_INITIAL;
    sm->transitions  = defaultTransitions;
    sm->callback     = nullptr;
    sm->version      = 0;

    return sm;
}

void bs_state_machine_destroy(StateMachine* sm)
{
    delete sm;
}

ConfigState bs_state_machine_get_current_state(const StateMachine* sm)
{
    if (!sm)
        return CONFIG_STATE_ERROR;
    return sm->currentState;
}

BS_NO_SANITIZE_ENUM int bs_state_machine_transition(StateMachine* sm, ConfigState newState)
{
    if (!sm)
        return -1;
    if (!isValidTransition(sm->currentState, newState))
        return -2;

    ConfigState oldState = sm->currentState;
    sm->currentState     = newState;
    sm->version++;

    if (sm->callback)
    {
        sm->callback(sm->configPath.c_str(), oldState, newState);
    }

    return 0;
}

BS_NO_SANITIZE_ENUM int bs_state_machine_can_transition(const StateMachine* sm,
                                                        ConfigState         newState)
{
    if (!sm)
        return 0;
    return isValidTransition(sm->currentState, newState);
}

void bs_state_machine_set_callback(StateMachine* sm, StateTransitionCallback callback)
{
    if (sm)
    {
        sm->callback = callback;
    }
}

uint64_t bs_state_machine_get_version(const StateMachine* sm)
{
    if (!sm)
        return 0;
    return sm->version;
}
