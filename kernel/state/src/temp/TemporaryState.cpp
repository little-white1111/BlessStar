#include "bs/kernel/state/StateBus.h"
#include "bs/kernel/state/StateMachine.h"
#include "bs/kernel/state/TemporaryState.h"

#include <cstdlib>
#include <cstring>
#include <ctime>

struct TemporaryState
{
    char*         path;
    char*         originalPath;
    void*         newConfig;
    size_t        configSize;
    StateMachine* sm;
    StateBus*     tempBus;
    bool          isActive;
    bool          isValidated;
};

TemporaryState* TemporaryState_Create(const char* path, const void* newConfig, size_t configSize)
{
    if (!path)
        return nullptr;

    TemporaryState* ts = new TemporaryState();
    if (!ts)
        return nullptr;

    ts->path         = strdup(path);
    ts->originalPath = strdup(path);
    ts->newConfig    = nullptr;
    ts->configSize   = 0;
    ts->isActive     = false;
    ts->isValidated  = false;
    ts->sm           = StateMachine_Create(path);
    ts->tempBus      = StateBus_Create();

    if (newConfig && configSize > 0)
    {
        ts->newConfig = malloc(configSize);
        if (ts->newConfig)
        {
            memcpy(ts->newConfig, newConfig, configSize);
            ts->configSize = configSize;
        }
    }

    if (!ts->sm || !ts->tempBus)
    {
        TemporaryState_Destroy(ts);
        return nullptr;
    }

    return ts;
}

void TemporaryState_Destroy(TemporaryState* ts)
{
    if (!ts)
        return;
    free(ts->path);
    free(ts->originalPath);
    if (ts->newConfig)
    {
        free(ts->newConfig);
    }
    StateMachine_Destroy(ts->sm);
    StateBus_Destroy(ts->tempBus);
    delete ts;
}

int TemporaryState_Activate(TemporaryState* ts)
{
    if (!ts)
        return -1;
    if (ts->isActive)
        return 0;

    int ret = StateMachine_Transition(ts->sm, CONFIG_STATE_LOADING);
    if (ret != 0)
        return -2;

    StateBus_SetState(ts->tempBus, ts->path, CONFIG_STATE_LOADING, ts->newConfig, ts->configSize);

    ret = StateMachine_Transition(ts->sm, CONFIG_STATE_ACTIVE);
    if (ret != 0)
        return -3;

    StateBus_SetState(ts->tempBus, ts->path, CONFIG_STATE_ACTIVE, ts->newConfig, ts->configSize);

    ts->isActive = true;
    return 0;
}

int TemporaryState_Validate(TemporaryState* ts)
{
    if (!ts || !ts->isActive)
        return -1;

    if (!ts->newConfig || ts->configSize == 0)
        return -2;

    ts->isValidated = true;
    return 0;
}

int TemporaryState_Commit(TemporaryState* ts)
{
    if (!ts || !ts->isActive || !ts->isValidated)
        return -1;

    int ret = StateMachine_Transition(ts->sm, CONFIG_STATE_CLOSED);
    if (ret != 0)
        return -2;

    ts->isActive = false;
    return 0;
}

int TemporaryState_Rollback(TemporaryState* ts)
{
    if (!ts)
        return -1;

    int ret = StateMachine_Transition(ts->sm, CONFIG_STATE_ERROR);
    if (ret != 0)
        StateMachine_Transition(ts->sm, CONFIG_STATE_CLOSED);
    else
        StateMachine_Transition(ts->sm, CONFIG_STATE_CLOSED);

    ts->isActive = false;
    return 0;
}

ConfigState TemporaryState_GetCurrentState(const TemporaryState* ts)
{
    if (!ts)
        return CONFIG_STATE_ERROR;
    return StateMachine_GetCurrentState(ts->sm);
}

int TemporaryState_IsActive(const TemporaryState* ts)
{
    if (!ts)
        return 0;
    return ts->isActive;
}
