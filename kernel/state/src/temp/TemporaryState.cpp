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

TemporaryState* bs_temporary_state_create(const char* path, const void* newConfig,
                                          size_t configSize)
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
    ts->sm           = bs_state_machine_create(path);
    ts->tempBus      = bs_state_bus_create();

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
        bs_temporary_state_destroy(ts);
        return nullptr;
    }

    return ts;
}

void bs_temporary_state_destroy(TemporaryState* ts)
{
    if (!ts)
        return;
    free(ts->path);
    free(ts->originalPath);
    if (ts->newConfig)
    {
        free(ts->newConfig);
    }
    bs_state_machine_destroy(ts->sm);
    bs_state_bus_destroy(ts->tempBus);
    delete ts;
}

int bs_temporary_state_activate(TemporaryState* ts)
{
    if (!ts)
        return -1;
    if (ts->isActive)
        return 0;

    int ret = bs_state_machine_transition(ts->sm, CONFIG_STATE_LOADING);
    if (ret != 0)
        return -2;

    bs_state_bus_set_state(ts->tempBus, ts->path, CONFIG_STATE_LOADING, ts->newConfig,
                           ts->configSize);

    ret = bs_state_machine_transition(ts->sm, CONFIG_STATE_ACTIVE);
    if (ret != 0)
        return -3;

    bs_state_bus_set_state(ts->tempBus, ts->path, CONFIG_STATE_ACTIVE, ts->newConfig,
                           ts->configSize);

    ts->isActive = true;
    return 0;
}

int bs_temporary_state_validate(TemporaryState* ts)
{
    if (!ts || !ts->isActive)
        return -1;

    if (!ts->newConfig || ts->configSize == 0)
        return -2;

    ts->isValidated = true;
    return 0;
}

int bs_temporary_state_commit(TemporaryState* ts)
{
    if (!ts || !ts->isActive || !ts->isValidated)
        return -1;

    int ret = bs_state_machine_transition(ts->sm, CONFIG_STATE_CLOSED);
    if (ret != 0)
        return -2;

    ts->isActive = false;
    return 0;
}

int bs_temporary_state_rollback(TemporaryState* ts)
{
    if (!ts)
        return -1;

    int ret = bs_state_machine_transition(ts->sm, CONFIG_STATE_ERROR);
    if (ret != 0)
        bs_state_machine_transition(ts->sm, CONFIG_STATE_CLOSED);
    else
        bs_state_machine_transition(ts->sm, CONFIG_STATE_CLOSED);

    ts->isActive = false;
    return 0;
}

ConfigState bs_temporary_state_get_current_state(const TemporaryState* ts)
{
    if (!ts)
        return CONFIG_STATE_ERROR;
    return bs_state_machine_get_current_state(ts->sm);
}

int bs_temporary_state_is_active(const TemporaryState* ts)
{
    if (!ts)
        return 0;
    return ts->isActive;
}
