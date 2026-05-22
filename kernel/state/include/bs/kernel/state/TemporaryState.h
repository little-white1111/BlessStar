#ifndef BS_KERNEL_STATE_TEMPORARYSTATE_H
#define BS_KERNEL_STATE_TEMPORARYSTATE_H

#include "ConfigState.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct TemporaryState TemporaryState;

    typedef struct StateBus StateBus;

    TemporaryState* TemporaryState_Create(const char* path, const void* newConfig,
                                          size_t configSize);

    void TemporaryState_Destroy(TemporaryState* ts);

    int TemporaryState_Activate(TemporaryState* ts);

    int TemporaryState_Validate(TemporaryState* ts);

    int TemporaryState_Commit(TemporaryState* ts);

    int TemporaryState_Rollback(TemporaryState* ts);

    ConfigState TemporaryState_GetCurrentState(const TemporaryState* ts);

    int TemporaryState_IsActive(const TemporaryState* ts);

#ifdef __cplusplus
}
#endif

#endif
