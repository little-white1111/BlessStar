#include "bs/kernel/common/bs_wait_trace.h"
#include "bs/kernel/ir/ir.h"
#include "bs/kernel/pipeline/Stage.h"
#include "bs/kernel/pipeline/pipeline.h"
#include "bs/kernel/report/report.h"
#include "bs/kernel/runtime/Kernel.h"
#include "bs/kernel/runtime/kernel_pool.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
typedef CRITICAL_SECTION   BsPoolMutex;
typedef CONDITION_VARIABLE BsPoolCond;
static void                bs_pool_mutex_init(BsPoolMutex* mu)
{
    InitializeCriticalSection(mu);
}
static void bs_pool_mutex_destroy(BsPoolMutex* mu)
{
    DeleteCriticalSection(mu);
}
static void bs_pool_mutex_lock(BsPoolMutex* mu)
{
    EnterCriticalSection(mu);
}
static void bs_pool_mutex_unlock(BsPoolMutex* mu)
{
    LeaveCriticalSection(mu);
}
static void bs_pool_cond_init(BsPoolCond* cv)
{
    InitializeConditionVariable(cv);
}
static void bs_pool_cond_destroy(BsPoolCond* cv)
{
    (void)cv;
}
static void bs_pool_cond_wait(BsPoolCond* cv, BsPoolMutex* mu)
{
    (void)SleepConditionVariableCS(cv, mu, INFINITE);
}
static void bs_pool_cond_broadcast(BsPoolCond* cv)
{
    WakeAllConditionVariable(cv);
}
#else
#include <pthread.h>
typedef pthread_mutex_t BsPoolMutex;
typedef pthread_cond_t  BsPoolCond;
static void             bs_pool_mutex_init(BsPoolMutex* mu)
{
    (void)pthread_mutex_init(mu, NULL);
}
static void bs_pool_mutex_destroy(BsPoolMutex* mu)
{
    (void)pthread_mutex_destroy(mu);
}
static void bs_pool_mutex_lock(BsPoolMutex* mu)
{
    (void)pthread_mutex_lock(mu);
}
static void bs_pool_mutex_unlock(BsPoolMutex* mu)
{
    (void)pthread_mutex_unlock(mu);
}
static void bs_pool_cond_init(BsPoolCond* cv)
{
    (void)pthread_cond_init(cv, NULL);
}
static void bs_pool_cond_destroy(BsPoolCond* cv)
{
    (void)pthread_cond_destroy(cv);
}
static void bs_pool_cond_wait(BsPoolCond* cv, BsPoolMutex* mu)
{
    (void)pthread_cond_wait(cv, mu);
}
static void bs_pool_cond_broadcast(BsPoolCond* cv)
{
    (void)pthread_cond_broadcast(cv);
}
#endif

#define BS_KERNEL_POOL_DEFAULT_STEADY 3u
#define BS_KERNEL_POOL_DEFAULT_MAX 10u
#define BS_KERNEL_POOL_DEFAULT_PRIORITY 1000u
#define BS_KERNEL_POOL_DEFAULT_DYNAMIC_DELTA 1u
#define BS_KERNEL_POOL_DEFAULT_IDLE_TTL_MS 1000u
#define BS_KERNEL_POOL_DEFAULT_INLINE_DEPTH 8u

typedef enum BsKernelPoolSlotKind
{
    BS_KERNEL_POOL_SLOT_STEADY  = 0,
    BS_KERNEL_POOL_SLOT_DYNAMIC = 1
} BsKernelPoolSlotKind;

typedef struct BsKernelPoolSlot
{
    BsKernelPoolSlotKind kind;
    uint32_t             priority;
    int                  busy;
    Kernel*              kernel;
    Pipeline*            pipeline;
} BsKernelPoolSlot;

struct BsKernelPool
{
    BsKernelPoolConfig config;
    BsKernelPoolSlot** slots;
    uint32_t           slot_count;
    uint32_t           slot_capacity;
    uint64_t           next_ticket;
    uint64_t           serving_ticket;
    uint32_t           waiters;
    int                draining;
    BsKernelPoolStats  stats;
    BsPoolMutex        mu;
    BsPoolCond         cv;
};

static int pool_pass_stage_execute(Stage* stage, const IRInstruction* input, IRInstruction** output)
{
    (void)stage;
    if (!input)
        return -1;
    if (output)
        *output = NULL;
    return 0;
}

void bs_kernel_pool_config_init_default(BsKernelPoolConfig* config)
{
    if (!config)
        return;
    memset(config, 0, sizeof(*config));
    config->steady_count               = BS_KERNEL_POOL_DEFAULT_STEADY;
    config->max_instances              = BS_KERNEL_POOL_DEFAULT_MAX;
    config->dynamic_idle_ttl_ms        = BS_KERNEL_POOL_DEFAULT_IDLE_TTL_MS;
    config->inline_depth_max           = BS_KERNEL_POOL_DEFAULT_INLINE_DEPTH;
    config->priority_steady            = BS_KERNEL_POOL_DEFAULT_PRIORITY;
    config->priority_dynamic_delta     = BS_KERNEL_POOL_DEFAULT_DYNAMIC_DELTA;
    config->fifo_wait_unbounded        = 1;
    config->per_batch_parallel_exec    = 1;
    config->slot_quarantine_on_failure = 0;
}

static void kernel_pool_destroy_slot(BsKernelPoolSlot* slot)
{
    if (!slot)
        return;
    if (slot->kernel)
    {
        if (bs_kernel_get_state(slot->kernel) == KERNEL_STATE_RUNNING)
            (void)bs_kernel_stop(slot->kernel);
        if (slot->pipeline)
            (void)bs_kernel_unregister_pipeline(slot->kernel, "default");
    }
    if (slot->pipeline)
        bs_pipeline_destroy(slot->pipeline);
    if (slot->kernel)
        bs_kernel_destroy(slot->kernel);
    free(slot);
}

static BsKernelPoolSlot* kernel_pool_create_slot(BsKernelPool* pool, BsKernelPoolSlotKind kind,
                                                 uint32_t dynamic_index)
{
    BsKernelPoolSlot* slot = (BsKernelPoolSlot*)calloc(1, sizeof(BsKernelPoolSlot));
    if (!slot)
        return NULL;
    slot->kind     = kind;
    slot->priority = pool->config.priority_steady;
    if (kind == BS_KERNEL_POOL_SLOT_DYNAMIC)
    {
        const uint32_t delta = pool->config.priority_dynamic_delta * (dynamic_index + 1u);
        slot->priority       = delta < slot->priority ? slot->priority - delta : 0u;
    }

    slot->kernel = bs_kernel_create(NULL);
    if (!slot->kernel)
    {
        kernel_pool_destroy_slot(slot);
        return NULL;
    }
    slot->pipeline = bs_pipeline_create();
    if (!slot->pipeline)
    {
        kernel_pool_destroy_slot(slot);
        return NULL;
    }
    Stage pass;
    memset(&pass, 0, sizeof(pass));
    pass.name    = "kernel_pool_default";
    pass.execute = pool_pass_stage_execute;
    if (bs_pipeline_add_stage(slot->pipeline, &pass) != 0 ||
        bs_kernel_register_pipeline(slot->kernel, "default", slot->pipeline) != 0 ||
        bs_kernel_start(slot->kernel) != 0)
    {
        kernel_pool_destroy_slot(slot);
        return NULL;
    }
    return slot;
}

static int kernel_pool_reserve_slot_array(BsKernelPool* pool, uint32_t needed)
{
    if (needed <= pool->slot_capacity)
        return 0;
    uint32_t new_capacity = pool->slot_capacity ? pool->slot_capacity * 2u : 4u;
    while (new_capacity < needed)
        new_capacity *= 2u;
    BsKernelPoolSlot** next =
        (BsKernelPoolSlot**)realloc(pool->slots, sizeof(BsKernelPoolSlot*) * new_capacity);
    if (!next)
        return BS_KERNEL_POOL_ERR_NOMEM;
    pool->slots         = next;
    pool->slot_capacity = new_capacity;
    return BS_KERNEL_POOL_OK;
}

static int kernel_pool_append_slot(BsKernelPool* pool, BsKernelPoolSlot* slot)
{
    int rc = kernel_pool_reserve_slot_array(pool, pool->slot_count + 1u);
    if (rc != BS_KERNEL_POOL_OK)
        return rc;
    pool->slots[pool->slot_count++] = slot;
    pool->stats.total_slots         = pool->slot_count;
    if (slot->kind == BS_KERNEL_POOL_SLOT_DYNAMIC)
        pool->stats.dynamic_slots++;
    return BS_KERNEL_POOL_OK;
}

BsKernelPool* bs_kernel_pool_create(const BsKernelPoolConfig* config)
{
    BsKernelPool* pool = (BsKernelPool*)calloc(1, sizeof(BsKernelPool));
    if (!pool)
        return NULL;
    if (config)
        pool->config = *config;
    else
        bs_kernel_pool_config_init_default(&pool->config);
    if (pool->config.steady_count == 0u)
        pool->config.steady_count = BS_KERNEL_POOL_DEFAULT_STEADY;
    if (pool->config.max_instances < pool->config.steady_count)
        pool->config.max_instances = pool->config.steady_count;
    pool->stats.steady_count  = pool->config.steady_count;
    pool->stats.max_instances = pool->config.max_instances;
    bs_pool_mutex_init(&pool->mu);
    bs_pool_cond_init(&pool->cv);
    return pool;
}

int bs_kernel_pool_warmup(BsKernelPool* pool)
{
    if (!pool)
        return BS_KERNEL_POOL_ERR_INVALID_ARG;
    bs_pool_mutex_lock(&pool->mu);
    if (pool->draining)
    {
        bs_pool_mutex_unlock(&pool->mu);
        return BS_KERNEL_POOL_ERR_STOPPING;
    }
    while (pool->slot_count < pool->config.steady_count)
    {
        BsKernelPoolSlot* slot = kernel_pool_create_slot(pool, BS_KERNEL_POOL_SLOT_STEADY, 0u);
        if (!slot)
        {
            BsKernelPoolSlot** rollback = pool->slots;
            uint32_t           count    = pool->slot_count;
            pool->slots                 = NULL;
            pool->slot_count            = 0u;
            pool->slot_capacity         = 0u;
            pool->stats.total_slots     = 0u;
            bs_pool_mutex_unlock(&pool->mu);
            for (uint32_t i = 0; i < count; i++)
                kernel_pool_destroy_slot(rollback[i]);
            free(rollback);
            return BS_KERNEL_POOL_ERR_NOMEM;
        }
        int rc = kernel_pool_append_slot(pool, slot);
        if (rc != BS_KERNEL_POOL_OK)
        {
            kernel_pool_destroy_slot(slot);
            bs_pool_mutex_unlock(&pool->mu);
            return rc;
        }
    }
    bs_pool_mutex_unlock(&pool->mu);
    return BS_KERNEL_POOL_OK;
}

static int kernel_pool_select_idle_slot(BsKernelPool* pool)
{
    int      selected = -1;
    uint32_t priority = 0u;
    for (uint32_t i = 0; i < pool->slot_count; i++)
    {
        BsKernelPoolSlot* slot = pool->slots[i];
        if (slot->busy)
            continue;
        if (selected < 0 || slot->priority > priority)
        {
            selected = (int)i;
            priority = slot->priority;
        }
    }
    return selected;
}

static void kernel_pool_remove_slot_at(BsKernelPool* pool, uint32_t index)
{
    BsKernelPoolSlot* slot = pool->slots[index];
    if (slot->kind == BS_KERNEL_POOL_SLOT_DYNAMIC && pool->stats.dynamic_slots > 0u)
        pool->stats.dynamic_slots--;
    for (uint32_t i = index; i + 1u < pool->slot_count; i++)
        pool->slots[i] = pool->slots[i + 1u];
    pool->slot_count--;
    pool->stats.total_slots = pool->slot_count;
}

int bs_kernel_pool_submit(BsKernelPool* pool, const IRInstruction* ir, Report** out_report)
{
    if (!pool || !ir || !out_report)
        return BS_KERNEL_POOL_ERR_INVALID_ARG;
    *out_report = NULL;
    if (bs_kernel_pool_warmup(pool) != BS_KERNEL_POOL_OK)
        return BS_KERNEL_POOL_ERR_STOPPING;

    bs_pool_mutex_lock(&pool->mu);
    if (pool->draining)
    {
        bs_pool_mutex_unlock(&pool->mu);
        return BS_KERNEL_POOL_ERR_STOPPING;
    }

    uint64_t my_ticket  = 0u;
    int      has_ticket = 0;
    int      selected   = -1;
    for (;;)
    {
        selected = kernel_pool_select_idle_slot(pool);
        if (selected >= 0 && (!has_ticket || my_ticket == pool->serving_ticket))
            break;
        if (pool->slot_count < pool->config.max_instances && !has_ticket)
        {
            BsKernelPoolSlot* slot = kernel_pool_create_slot(pool, BS_KERNEL_POOL_SLOT_DYNAMIC,
                                                             pool->stats.dynamic_slots);
            if (!slot)
            {
                bs_pool_mutex_unlock(&pool->mu);
                return BS_KERNEL_POOL_ERR_NOMEM;
            }
            int rc = kernel_pool_append_slot(pool, slot);
            if (rc != BS_KERNEL_POOL_OK)
            {
                kernel_pool_destroy_slot(slot);
                bs_pool_mutex_unlock(&pool->mu);
                return rc;
            }
            selected = (int)(pool->slot_count - 1u);
            break;
        }
        if (!has_ticket)
        {
            my_ticket  = pool->next_ticket++;
            has_ticket = 1;
            pool->waiters++;
        }
        {
            const int hang_t0 = bs_wait_trace_hang_begin("kernel_pool_submit:wait_pool_cv");
            while (!pool->draining &&
                   (my_ticket != pool->serving_ticket || kernel_pool_select_idle_slot(pool) < 0))
            {
                bs_wait_trace_hang_tick_u64("kernel_pool_submit:wait_pool_cv", hang_t0,
                                            (unsigned long long)pool->stats.busy_slots);
                bs_pool_cond_wait(&pool->cv, &pool->mu);
            }
        }
        if (pool->draining)
        {
            if (pool->waiters > 0u)
                pool->waiters--;
            bs_pool_mutex_unlock(&pool->mu);
            return BS_KERNEL_POOL_ERR_STOPPING;
        }
    }

    BsKernelPoolSlot* slot = pool->slots[selected];
    slot->busy             = 1;
    pool->stats.busy_slots++;
    pool->stats.submitted_jobs++;
    if (has_ticket)
    {
        if (pool->waiters > 0u)
            pool->waiters--;
        pool->serving_ticket++;
        bs_pool_cond_broadcast(&pool->cv);
    }
    bs_pool_mutex_unlock(&pool->mu);

    Report* report = bs_kernel_execute(slot->kernel, ir);

    BsKernelPoolSlot* destroy_slot = NULL;
    bs_pool_mutex_lock(&pool->mu);
    slot->busy = 0;
    if (pool->stats.busy_slots > 0u)
        pool->stats.busy_slots--;
    if (report)
    {
        pool->stats.completed_jobs++;
        *out_report = report;
    }
    else
    {
        pool->stats.failed_jobs++;
    }
    if (!slot->busy && slot->kind == BS_KERNEL_POOL_SLOT_DYNAMIC && pool->waiters == 0u &&
        pool->slot_count > pool->config.steady_count)
    {
        for (uint32_t i = 0; i < pool->slot_count; i++)
        {
            if (pool->slots[i] == slot)
            {
                kernel_pool_remove_slot_at(pool, i);
                destroy_slot = slot;
                break;
            }
        }
    }
    bs_pool_cond_broadcast(&pool->cv);
    bs_pool_mutex_unlock(&pool->mu);

    kernel_pool_destroy_slot(destroy_slot);
    return report ? BS_KERNEL_POOL_OK : BS_KERNEL_POOL_ERR_EXEC_FAILED;
}

int bs_kernel_pool_get_stats(BsKernelPool* pool, BsKernelPoolStats* out_stats)
{
    if (!pool || !out_stats)
        return BS_KERNEL_POOL_ERR_INVALID_ARG;
    bs_pool_mutex_lock(&pool->mu);
    *out_stats = pool->stats;
    bs_pool_mutex_unlock(&pool->mu);
    return BS_KERNEL_POOL_OK;
}

int bs_kernel_pool_reset_all_pipelines(BsKernelPool* pool)
{
    if (!pool)
        return BS_KERNEL_POOL_ERR_INVALID_ARG;

    bs_pool_mutex_lock(&pool->mu);
    {
        const int hang_t0 = bs_wait_trace_hang_begin("kernel_pool_reset:wait_busy_slots");
        while (pool->stats.busy_slots > 0u)
        {
            bs_wait_trace_hang_tick_u64("kernel_pool_reset:wait_busy_slots", hang_t0,
                                        (unsigned long long)pool->stats.busy_slots);
            bs_pool_cond_wait(&pool->cv, &pool->mu);
        }
    }

    int rc = BS_KERNEL_POOL_OK;
    for (uint32_t i = 0; i < pool->slot_count; i++)
    {
        BsKernelPoolSlot* slot = pool->slots[i];
        if (slot && slot->pipeline && bs_pipeline_reset(slot->pipeline) != 0)
            rc = BS_KERNEL_POOL_ERR_EXEC_FAILED;
    }

    bs_pool_mutex_unlock(&pool->mu);
    return rc;
}

#if defined(BS_TESTING)
uint32_t bs_kernel_pool_testing_count_non_idle_stages(BsKernelPool* pool)
{
    if (!pool)
        return 0u;
    uint32_t non_idle = 0u;
    bs_pool_mutex_lock(&pool->mu);
    for (uint32_t i = 0; i < pool->slot_count; i++)
    {
        BsKernelPoolSlot* slot = pool->slots[i];
        if (!slot || !slot->pipeline)
            continue;
        for (Stage* stage = slot->pipeline->stages; stage; stage = stage->next)
        {
            if (bs_stage_get_state(stage) != STAGE_STATE_IDLE)
                non_idle++;
        }
    }
    bs_pool_mutex_unlock(&pool->mu);
    return non_idle;
}
#endif

void bs_kernel_pool_destroy(BsKernelPool* pool)
{
    if (!pool)
        return;
    bs_pool_mutex_lock(&pool->mu);
    pool->draining = 1;
    bs_pool_cond_broadcast(&pool->cv);
    while (pool->stats.busy_slots > 0u)
        bs_pool_cond_wait(&pool->cv, &pool->mu);
    BsKernelPoolSlot** slots = pool->slots;
    uint32_t           count = pool->slot_count;
    pool->slots              = NULL;
    pool->slot_count         = 0u;
    pool->slot_capacity      = 0u;
    bs_pool_mutex_unlock(&pool->mu);

    for (uint32_t i = 0; i < count; i++)
        kernel_pool_destroy_slot(slots[i]);
    free(slots);
    bs_pool_cond_destroy(&pool->cv);
    bs_pool_mutex_destroy(&pool->mu);
    free(pool);
}
