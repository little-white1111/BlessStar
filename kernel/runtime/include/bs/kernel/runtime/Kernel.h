#ifndef BS_KERNEL_RUNTIME_KERNEL_H
#define BS_KERNEL_RUNTIME_KERNEL_H

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

    Kernel* kernel_create(const KernelConfig* config);
    void    kernel_destroy(Kernel* kernel);

    int         kernel_start(Kernel* kernel);
    int         kernel_stop(Kernel* kernel);
    KernelState kernel_get_state(const Kernel* kernel);

    Report* kernel_execute(Kernel* kernel, const IRInstruction* ir);
    int     kernel_execute_async(Kernel* kernel, const IRInstruction* ir);

    int   kernel_register_pipeline(Kernel* kernel, const char* name, void* pipeline);
    int   kernel_unregister_pipeline(Kernel* kernel, const char* name);
    void* kernel_get_pipeline(Kernel* kernel, const char* name);

    int                 kernel_set_config(Kernel* kernel, const KernelConfig* config);
    const KernelConfig* kernel_get_config(const Kernel* kernel);

    const char* kernel_get_version(void);
    uint64_t    kernel_get_start_time(const Kernel* kernel);
    uint64_t    kernel_get_execution_count(const Kernel* kernel);

#ifdef __cplusplus
}
#endif

#endif // BS_KERNEL_RUNTIME_KERNEL_H
