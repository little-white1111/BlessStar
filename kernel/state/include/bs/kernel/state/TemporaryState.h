#ifndef BS_KERNEL_STATE_TEMPORARYSTATE_H
#define BS_KERNEL_STATE_TEMPORARYSTATE_H

/*
 * C-ST-7 contract block:
 * Thread safety: Not thread-safe; scoped to attach/reload session.
 * Error semantics: Non-zero on invalid session or exhausted temp bus capacity.
 * Platform notes: Uses nested StateBus for short-lived staging during reload.
 */

#include "ConfigState.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct TemporaryState TemporaryState;

    typedef struct StateBus StateBus;

    TemporaryState* bs_temporary_state_create(const char* path, const void* newConfig,
                                              size_t configSize);

    void bs_temporary_state_destroy(TemporaryState* ts);

    int bs_temporary_state_activate(TemporaryState* ts);

    int bs_temporary_state_validate(TemporaryState* ts);

    int bs_temporary_state_commit(TemporaryState* ts);

    int bs_temporary_state_rollback(TemporaryState* ts);

    ConfigState bs_temporary_state_get_current_state(const TemporaryState* ts);

    int bs_temporary_state_is_active(const TemporaryState* ts);

#ifdef __cplusplus
}
#endif

#endif
