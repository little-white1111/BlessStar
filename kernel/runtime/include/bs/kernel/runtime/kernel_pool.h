#ifndef BS_KERNEL_RUNTIME_KERNEL_POOL_H
#define BS_KERNEL_RUNTIME_KERNEL_POOL_H

/*
 * C-ST-7 contract block:
 * Thread safety: submit is multi-producer safe; each slot serializes execution via Kernel worker.
 * Error semantics: returns BS_KERNEL_POOL_* codes; Report* ownership transfers to caller on success.
 * Platform notes: runtime-only pool; must not depend on adapter or ConfigManager.
 *
 * C-KERNEL-POOL-1 contract block:
 * Gate: GATE-KERNEL-POOL-CONFIG (steady=3, max=10, inline_depth_max=8).
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct IRInstruction IRInstruction;
    typedef struct Report        Report;
    typedef struct BsKernelPool  BsKernelPool;

    enum
    {
        BS_KERNEL_POOL_OK              = 0,
        BS_KERNEL_POOL_ERR_INVALID_ARG = -1,
        BS_KERNEL_POOL_ERR_NOMEM       = -2,
        BS_KERNEL_POOL_ERR_STOPPING    = -3,
        BS_KERNEL_POOL_ERR_EXEC_FAILED = -4
    };

    typedef struct BsKernelPoolConfig
    {
        uint32_t steady_count;
        uint32_t max_instances;
        uint32_t dynamic_idle_ttl_ms;
        uint32_t inline_depth_max;
        uint32_t priority_steady;
        uint32_t priority_dynamic_delta;
        int      fifo_wait_unbounded;
        int      per_batch_parallel_exec;
        int      slot_quarantine_on_failure;
    } BsKernelPoolConfig;

    typedef struct BsKernelPoolStats
    {
        uint32_t steady_count;
        uint32_t max_instances;
        uint32_t total_slots;
        uint32_t busy_slots;
        uint32_t dynamic_slots;
        uint64_t submitted_jobs;
        uint64_t completed_jobs;
        uint64_t failed_jobs;
    } BsKernelPoolStats;

    void bs_kernel_pool_config_init_default(BsKernelPoolConfig* config);

    BsKernelPool* bs_kernel_pool_create(const BsKernelPoolConfig* config);
    int           bs_kernel_pool_warmup(BsKernelPool* pool);
    int bs_kernel_pool_submit(BsKernelPool* pool, const IRInstruction* ir, Report** out_report);
    int bs_kernel_pool_get_stats(BsKernelPool* pool, BsKernelPoolStats* out_stats);
    void bs_kernel_pool_destroy(BsKernelPool* pool);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_RUNTIME_KERNEL_POOL_H */
