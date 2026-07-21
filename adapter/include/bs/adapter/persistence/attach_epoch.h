#ifndef BS_ADAPTER_PERSISTENCE_ATTACH_EPOCH_H
#define BS_ADAPTER_PERSISTENCE_ATTACH_EPOCH_H

/*
 * C-EPOCH: forward declaration of EpochState.
 * Full definition lives in test files (*.cpp) that were intentionally omitted
 * from this branch. Production code only uses bs_epoch_state_t* via pointer
 * (attach_context.h:bs_adapter_attach_ctx_epoch_state).
 */

typedef struct bs_epoch_state bs_epoch_state_t;

#endif /* BS_ADAPTER_PERSISTENCE_ATTACH_EPOCH_H */
