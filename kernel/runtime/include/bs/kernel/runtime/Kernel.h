#ifndef BS_KERNEL_RUNTIME_KERNEL_H
#define BS_KERNEL_RUNTIME_KERNEL_H

/*
 * C-ST-7 contract block:
 * Thread safety: Kernel instance is not thread-safe across threads.
 * Error semantics: NULL Kernel* on create failure; async execute enqueues IR copies for drain.
 * Platform notes: Optional runtime orchestrator; links report + pipeline registries.
 *   Pipeline pointers are caller-owned; destroy unregisters names only (see attach teardown).
 *   bs_kernel_execute uses pipeline name "default" when registered (XVII-KERNEL-4/5).
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct IRInstruction IRInstruction;
    typedef struct Report        Report;
    typedef struct KernelConfig  KernelConfig;
    typedef struct Kernel        Kernel;

    typedef enum KernelState
    {
        KERNEL_STATE_STOPPED,
        KERNEL_STATE_STARTING,
        KERNEL_STATE_RUNNING,
        KERNEL_STATE_STOPPING,
        KERNEL_STATE_ERROR
    } KernelState;

    Kernel* bs_kernel_create(const KernelConfig* config);
    void    bs_kernel_destroy(Kernel* kernel);

    int         bs_kernel_start(Kernel* kernel);
    int         bs_kernel_stop(Kernel* kernel);
    KernelState bs_kernel_get_state(const Kernel* kernel);

    Report* bs_kernel_execute(Kernel* kernel, const IRInstruction* ir);
    int     bs_kernel_execute_async(Kernel* kernel, const IRInstruction* ir);
    /** Drain queued async IR; returns count processed or -1 on execute failure. */
    int bs_kernel_drain_async_queue(Kernel* kernel);

    int   bs_kernel_register_pipeline(Kernel* kernel, const char* name, void* pipeline);
    int   bs_kernel_unregister_pipeline(Kernel* kernel, const char* name);
    void* bs_kernel_get_pipeline(Kernel* kernel, const char* name);

    int                 bs_kernel_set_config(Kernel* kernel, const KernelConfig* config);
    const KernelConfig* bs_kernel_get_config(const Kernel* kernel);

    const char* bs_kernel_get_version(void);
    uint64_t    bs_kernel_get_start_time(const Kernel* kernel);
    uint64_t    bs_kernel_get_execution_count(const Kernel* kernel);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_RUNTIME_KERNEL_H */
