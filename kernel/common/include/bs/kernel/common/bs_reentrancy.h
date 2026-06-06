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

    int bs_reentrancy_in_attach_write(void);

    void bs_reentrancy_enter_attach_write(void);
    void bs_reentrancy_leave_attach_write(void);

    int bs_reentrancy_kernel_execute_depth(void);

    void bs_reentrancy_enter_kernel_execute(void);
    void bs_reentrancy_leave_kernel_execute(void);

    /** Debug trap when listener attempts attach write (T20.8). No-op in release. */
    void bs_reentrancy_trap_listener_write_violation(void);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_COMMON_BS_REENTRANCY_H */
