#ifndef BS_KERNEL_COMMON_BS_REENTRANCY_H
#define BS_KERNEL_COMMON_BS_REENTRANCY_H

/*
 * C-ST-7 contract block:
 * Thread safety: Thread-local depth counters for nested callbacks.
 * Error semantics: N/A (markers only).
 * Platform notes: Guards EventBus listener paths (ConfigManager via attach_config).
 */

#ifdef __cplusplus
extern "C"
{
#endif

    int bs_reentrancy_in_state_callback(void);

    void bs_reentrancy_enter_state_callback(void);
    void bs_reentrancy_leave_state_callback(void);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_COMMON_BS_REENTRANCY_H */
