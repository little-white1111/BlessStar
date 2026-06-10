#include "bs/kernel/common/bs_reentrancy.h"
#include "bs/kernel/ir/ir.h"
#include "bs/kernel/pipeline/pipeline.h"
#include "bs/kernel/report/report.h"
#include "bs/kernel/runtime/Config.h"
#include "bs/kernel/runtime/Context.h"
#include "bs/kernel/runtime/Kernel.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
typedef HANDLE             BsThread;
typedef CRITICAL_SECTION   BsMutex;
typedef CONDITION_VARIABLE BsCond;
static void                bs_mutex_init(BsMutex* mu)
{
    InitializeCriticalSection(mu);
}
static void bs_mutex_destroy(BsMutex* mu)
{
    DeleteCriticalSection(mu);
}
static void bs_mutex_lock(BsMutex* mu)
{
    EnterCriticalSection(mu);
}
static void bs_mutex_unlock(BsMutex* mu)
{
    LeaveCriticalSection(mu);
}
static void bs_cond_init(BsCond* cv)
{
    InitializeConditionVariable(cv);
}
static void bs_cond_destroy(BsCond* cv)
{
    (void)cv;
}
static void bs_cond_wait(BsCond* cv, BsMutex* mu)
{
    SleepConditionVariableCS(cv, mu, INFINITE);
}
static void bs_cond_signal(BsCond* cv)
{
    WakeConditionVariable(cv);
}
static void bs_cond_broadcast(BsCond* cv)
{
    WakeAllConditionVariable(cv);
}
#else
#include <pthread.h>
typedef pthread_t       BsThread;
typedef pthread_mutex_t BsMutex;
typedef pthread_cond_t  BsCond;
static void             bs_mutex_init(BsMutex* mu)
{
    (void)pthread_mutex_init(mu, NULL);
}
static void bs_mutex_destroy(BsMutex* mu)
{
    (void)pthread_mutex_destroy(mu);
}
static void bs_mutex_lock(BsMutex* mu)
{
    (void)pthread_mutex_lock(mu);
}
static void bs_mutex_unlock(BsMutex* mu)
{
    (void)pthread_mutex_unlock(mu);
}
static void bs_cond_init(BsCond* cv)
{
    (void)pthread_cond_init(cv, NULL);
}
static void bs_cond_destroy(BsCond* cv)
{
    (void)pthread_cond_destroy(cv);
}
static void bs_cond_wait(BsCond* cv, BsMutex* mu)
{
    (void)pthread_cond_wait(cv, mu);
}
static void bs_cond_signal(BsCond* cv)
{
    (void)pthread_cond_signal(cv);
}
static void bs_cond_broadcast(BsCond* cv)
{
    (void)pthread_cond_broadcast(cv);
}
#endif

#define PIPELINE_INITIAL_CAPACITY 4
#define KERNEL_ASYNC_QUEUE_CAP 32
#define BS_KERNEL_EXEC_INLINE_DEPTH_MAX 2

typedef struct KernelAsyncNode
{
    IRInstruction*          ir;
    struct KernelAsyncNode* next;
} KernelAsyncNode;

typedef struct PipelineEntry
{
    const char* name;
    void*       pipeline;
    size_t      active_jobs;
    int         accepting;
} PipelineEntry;

typedef struct KernelExecJob
{
    PipelineEntry*        pipeline_ref;
    const IRInstruction*  ir;
    Report*               report;
    int                   result;
    int                   done;
    BsCond                done_cv;
    struct KernelExecJob* next;
} KernelExecJob;

struct Kernel
{
    KernelState      state;
    KernelConfig*    config;
    Context*         context;
    PipelineEntry*   pipelines;
    size_t           pipeline_count;
    size_t           pipeline_capacity;
    uint64_t         start_time;
    uint64_t         execution_count;
    KernelAsyncNode* async_head;
    KernelAsyncNode* async_tail;
    size_t           async_count;
    BsMutex          registry_mu;
    BsCond           registry_cv;
    BsMutex          exec_mu;
    BsCond           exec_cv;
    BsThread         exec_thread;
    int              exec_thread_started;
    int              exec_thread_ready;
    int              exec_stop_requested;
    KernelExecJob*   exec_head;
    KernelExecJob*   exec_tail;
};

static const char* KERNEL_VERSION = "1.0.0";

static void kernel_release_pipeline_ref(Kernel* kernel, PipelineEntry* ref)
{
    if (!kernel || !ref)
        return;
    bs_mutex_lock(&kernel->registry_mu);
    if (ref->active_jobs > 0)
        ref->active_jobs--;
    bs_cond_broadcast(&kernel->registry_cv);
    bs_mutex_unlock(&kernel->registry_mu);
}

static PipelineEntry* kernel_acquire_pipeline_ref(Kernel* kernel)
{
    if (!kernel)
        return NULL;

    bs_mutex_lock(&kernel->registry_mu);
    PipelineEntry* selected = NULL;
    for (size_t i = 0; i < kernel->pipeline_count; i++)
    {
        if (kernel->pipelines[i].accepting && kernel->pipelines[i].name &&
            strcmp(kernel->pipelines[i].name, "default") == 0)
        {
            selected = &kernel->pipelines[i];
            break;
        }
    }
    if (!selected)
    {
        for (size_t i = 0; i < kernel->pipeline_count; i++)
        {
            if (kernel->pipelines[i].accepting)
            {
                selected = &kernel->pipelines[i];
                break;
            }
        }
    }
    if (selected)
        selected->active_jobs++;
    bs_mutex_unlock(&kernel->registry_mu);
    return selected;
}

static Report* kernel_execute_with_ref(Kernel* kernel, PipelineEntry* ref, const IRInstruction* ir)
{
    if (!kernel || !ref || !ref->pipeline || !ir)
        return NULL;

    Report* pipe_report = NULL;
    bs_reentrancy_enter_kernel_execute();
    (void)bs_pipeline_reset((Pipeline*)ref->pipeline);
    const int rc = bs_pipeline_execute((Pipeline*)ref->pipeline, ir, &pipe_report);
    bs_reentrancy_leave_kernel_execute();
    if (rc != 0)
    {
        if (pipe_report)
            bs_report_destroy(pipe_report);
        return NULL;
    }

    kernel->execution_count++;
    return pipe_report;
}

static void kernel_exec_enqueue(Kernel* kernel, KernelExecJob* job)
{
    job->next = NULL;
    if (!kernel->exec_tail)
        kernel->exec_head = kernel->exec_tail = job;
    else
    {
        kernel->exec_tail->next = job;
        kernel->exec_tail       = job;
    }
    bs_cond_signal(&kernel->exec_cv);
}

static KernelExecJob* kernel_exec_pop(Kernel* kernel)
{
    KernelExecJob* job = kernel->exec_head;
    if (!job)
        return NULL;
    kernel->exec_head = job->next;
    if (!kernel->exec_head)
        kernel->exec_tail = NULL;
    job->next = NULL;
    return job;
}

#ifdef _WIN32
static DWORD WINAPI kernel_executor_worker_main(LPVOID arg)
#else
static void* kernel_executor_worker_main(void* arg)
#endif
{
    Kernel* kernel = (Kernel*)arg;
    bs_mutex_lock(&kernel->exec_mu);
    kernel->exec_thread_ready = 1;
    bs_cond_broadcast(&kernel->exec_cv);
    bs_mutex_unlock(&kernel->exec_mu);

    for (;;)
    {
        bs_mutex_lock(&kernel->exec_mu);
        while (!kernel->exec_head && !kernel->exec_stop_requested)
            bs_cond_wait(&kernel->exec_cv, &kernel->exec_mu);
        if (!kernel->exec_head && kernel->exec_stop_requested)
        {
            bs_mutex_unlock(&kernel->exec_mu);
            break;
        }
        KernelExecJob* job = kernel_exec_pop(kernel);
        bs_mutex_unlock(&kernel->exec_mu);

        job->report = kernel_execute_with_ref(kernel, job->pipeline_ref, job->ir);
        job->result = job->report ? 0 : -1;
        kernel_release_pipeline_ref(kernel, job->pipeline_ref);

        bs_mutex_lock(&kernel->exec_mu);
        job->done = 1;
        bs_cond_signal(&job->done_cv);
        bs_mutex_unlock(&kernel->exec_mu);
    }
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int kernel_start_executor(Kernel* kernel)
{
    if (!kernel || kernel->exec_thread_started)
        return 0;
    bs_mutex_lock(&kernel->exec_mu);
    kernel->exec_thread_ready   = 0;
    kernel->exec_stop_requested = 0;
    kernel->exec_thread_started = 0;
    bs_mutex_unlock(&kernel->exec_mu);
#ifdef _WIN32
    kernel->exec_thread = CreateThread(NULL, 0, kernel_executor_worker_main, kernel, 0, NULL);
    if (!kernel->exec_thread)
        return -1;
#else
    if (pthread_create(&kernel->exec_thread, NULL, kernel_executor_worker_main, kernel) != 0)
        return -1;
#endif
    bs_mutex_lock(&kernel->exec_mu);
    kernel->exec_thread_started = 1;
    while (!kernel->exec_thread_ready)
        bs_cond_wait(&kernel->exec_cv, &kernel->exec_mu);
    bs_mutex_unlock(&kernel->exec_mu);
    return 0;
}

static void kernel_stop_executor(Kernel* kernel)
{
    if (!kernel || !kernel->exec_thread_started)
        return;
    bs_mutex_lock(&kernel->exec_mu);
    kernel->exec_stop_requested = 1;
    bs_cond_broadcast(&kernel->exec_cv);
    bs_mutex_unlock(&kernel->exec_mu);
#ifdef _WIN32
    WaitForSingleObject(kernel->exec_thread, INFINITE);
    CloseHandle(kernel->exec_thread);
    kernel->exec_thread = NULL;
#else
    (void)pthread_join(kernel->exec_thread, NULL);
#endif
    kernel->exec_thread_ready   = 0;
    kernel->exec_thread_started = 0;
}

Kernel* bs_kernel_create(const KernelConfig* config)
{
    Kernel* kernel = (Kernel*)malloc(sizeof(Kernel));
    if (!kernel)
        return NULL;

    kernel->state = KERNEL_STATE_STOPPED;

    if (config)
    {
        kernel->config = (KernelConfig*)malloc(sizeof(KernelConfig));
        if (!kernel->config)
        {
            free(kernel);
            return NULL;
        }
        memcpy(kernel->config, config, sizeof(KernelConfig));
    }
    else
    {
        kernel->config = bs_kernel_config_create();
        if (!kernel->config)
        {
            free(kernel);
            return NULL;
        }
    }

    kernel->context = bs_context_create(CONTEXT_SCOPE_GLOBAL);
    if (!kernel->context)
    {
        if (kernel->config)
            bs_kernel_config_destroy(kernel->config);
        free(kernel);
        return NULL;
    }

    kernel->pipelines = (PipelineEntry*)malloc(sizeof(PipelineEntry) * PIPELINE_INITIAL_CAPACITY);
    if (!kernel->pipelines)
    {
        bs_context_destroy(kernel->context);
        if (kernel->config)
            bs_kernel_config_destroy(kernel->config);
        free(kernel);
        return NULL;
    }

    kernel->pipeline_count    = 0;
    kernel->pipeline_capacity = PIPELINE_INITIAL_CAPACITY;
    kernel->start_time        = 0;
    kernel->execution_count   = 0;
    kernel->async_head        = NULL;
    kernel->async_tail        = NULL;
    kernel->async_count       = 0;
    bs_mutex_init(&kernel->registry_mu);
    bs_cond_init(&kernel->registry_cv);
    bs_mutex_init(&kernel->exec_mu);
    bs_cond_init(&kernel->exec_cv);
    kernel->exec_thread_started = 0;
    kernel->exec_thread_ready   = 0;
    kernel->exec_stop_requested = 0;
    kernel->exec_head           = NULL;
    kernel->exec_tail           = NULL;

    return kernel;
}

void bs_kernel_destroy(Kernel* kernel)
{
    if (!kernel)
        return;

    if (kernel->state == KERNEL_STATE_RUNNING || kernel->state == KERNEL_STATE_STOPPING)
        (void)bs_kernel_stop(kernel);

    for (size_t i = 0; i < kernel->pipeline_count; i++)
    {
        if (kernel->pipelines[i].name)
        {
            free((void*)kernel->pipelines[i].name);
        }
        /* Pipeline objects are caller-owned (AttachContext destroys default_pipeline
         * after bs_kernel_unregister_pipeline). Do not bs_pipeline_destroy here. */
    }

    if (kernel->pipelines)
        free(kernel->pipelines);
    if (kernel->context)
        bs_context_destroy(kernel->context);
    if (kernel->config)
        bs_kernel_config_destroy(kernel->config);

    KernelAsyncNode* node = kernel->async_head;
    while (node)
    {
        KernelAsyncNode* next = node->next;
        if (node->ir)
            bs_ir_instruction_destroy(node->ir);
        free(node);
        node = next;
    }

    bs_cond_destroy(&kernel->exec_cv);
    bs_mutex_destroy(&kernel->exec_mu);
    bs_cond_destroy(&kernel->registry_cv);
    bs_mutex_destroy(&kernel->registry_mu);

    free(kernel);
}

static IRInstruction* kernel_copy_instruction(const IRInstruction* ir)
{
    if (!ir)
        return NULL;
    IRInstruction* copy = bs_ir_instruction_create(ir->type, ir->name);
    if (!copy)
        return NULL;
    copy->version   = ir->version;
    copy->timestamp = ir->timestamp;
    return copy;
}

static void kernel_async_enqueue(Kernel* kernel, IRInstruction* ir)
{
    KernelAsyncNode* node = (KernelAsyncNode*)malloc(sizeof(KernelAsyncNode));
    if (!node)
    {
        bs_ir_instruction_destroy(ir);
        return;
    }
    node->ir   = ir;
    node->next = NULL;
    if (!kernel->async_tail)
        kernel->async_head = kernel->async_tail = node;
    else
    {
        kernel->async_tail->next = node;
        kernel->async_tail       = node;
    }
    kernel->async_count++;
}

int bs_kernel_start(Kernel* kernel)
{
    if (!kernel)
        return -1;
    if (kernel->state != KERNEL_STATE_STOPPED)
        return -1;

    kernel->state = KERNEL_STATE_STARTING;

    // Initialize components
    bs_context_set_metadata(kernel->context, "kernel.version", KERNEL_VERSION);
    if (kernel_start_executor(kernel) != 0)
    {
        kernel->state = KERNEL_STATE_ERROR;
        return -1;
    }

    kernel->start_time = (uint64_t)time(NULL);
    kernel->state      = KERNEL_STATE_RUNNING;

    return 0;
}

int bs_kernel_stop(Kernel* kernel)
{
    if (!kernel)
        return -1;
    if (kernel->state != KERNEL_STATE_RUNNING)
        return -1;

    kernel->state = KERNEL_STATE_STOPPING;

    kernel_stop_executor(kernel);
    kernel->execution_count = 0;

    kernel->state = KERNEL_STATE_STOPPED;

    return 0;
}

KernelState bs_kernel_get_state(const Kernel* kernel)
{
    return kernel ? kernel->state : KERNEL_STATE_ERROR;
}

Report* bs_kernel_execute(Kernel* kernel, const IRInstruction* ir)
{
    if (!kernel || !ir)
        return NULL;
    if (kernel->state != KERNEL_STATE_RUNNING)
        return NULL;

    PipelineEntry* pipeline_ref = kernel_acquire_pipeline_ref(kernel);
    if (!pipeline_ref)
        return NULL;

    if (bs_reentrancy_kernel_execute_depth() > 0)
    {
        if (bs_reentrancy_kernel_execute_depth() >= BS_KERNEL_EXEC_INLINE_DEPTH_MAX)
        {
            kernel_release_pipeline_ref(kernel, pipeline_ref);
            return NULL;
        }
        Report* inline_report = kernel_execute_with_ref(kernel, pipeline_ref, ir);
        kernel_release_pipeline_ref(kernel, pipeline_ref);
        return inline_report;
    }

    KernelExecJob job;
    memset(&job, 0, sizeof(job));
    job.pipeline_ref = pipeline_ref;
    job.ir           = ir;
    bs_cond_init(&job.done_cv);

    bs_mutex_lock(&kernel->exec_mu);
    if (kernel->exec_stop_requested || !kernel->exec_thread_started)
    {
        bs_mutex_unlock(&kernel->exec_mu);
        bs_cond_destroy(&job.done_cv);
        kernel_release_pipeline_ref(kernel, pipeline_ref);
        return NULL;
    }
    kernel_exec_enqueue(kernel, &job);
    while (!job.done)
        bs_cond_wait(&job.done_cv, &kernel->exec_mu);
    bs_mutex_unlock(&kernel->exec_mu);

    bs_cond_destroy(&job.done_cv);
    return job.result == 0 ? job.report : NULL;
}

int bs_kernel_execute_async(Kernel* kernel, const IRInstruction* ir)
{
    if (!kernel || !ir)
        return -1;
    if (kernel->state != KERNEL_STATE_RUNNING)
        return -1;
    if (kernel->async_count >= KERNEL_ASYNC_QUEUE_CAP)
        return -1;

    IRInstruction* copy = kernel_copy_instruction(ir);
    if (!copy)
        return -1;

    kernel_async_enqueue(kernel, copy);
    return 0;
}

int bs_kernel_drain_async_queue(Kernel* kernel)
{
    if (!kernel)
        return -1;
    if (kernel->state != KERNEL_STATE_RUNNING)
        return -1;

    int processed = 0;
    while (kernel->async_head)
    {
        KernelAsyncNode* node = kernel->async_head;
        kernel->async_head    = node->next;
        if (!kernel->async_head)
            kernel->async_tail = NULL;
        kernel->async_count--;

        Report* report = bs_kernel_execute(kernel, node->ir);
        if (!report || bs_report_get_status(report) != REPORT_STATUS_SUCCESS)
        {
            if (report)
                bs_report_destroy(report);
            bs_ir_instruction_destroy(node->ir);
            free(node);
            return -1;
        }
        bs_report_destroy(report);
        bs_ir_instruction_destroy(node->ir);
        free(node);
        processed++;
    }
    return processed;
}

static int kernel_resize_pipelines(Kernel* kernel, size_t new_capacity)
{
    for (size_t i = 0; i < kernel->pipeline_count; i++)
    {
        while (kernel->pipelines[i].active_jobs > 0)
            bs_cond_wait(&kernel->registry_cv, &kernel->registry_mu);
    }
    PipelineEntry* new_pipelines =
        (PipelineEntry*)realloc(kernel->pipelines, sizeof(PipelineEntry) * new_capacity);
    if (!new_pipelines)
        return -1;

    kernel->pipelines         = new_pipelines;
    kernel->pipeline_capacity = new_capacity;
    return 0;
}

int bs_kernel_register_pipeline(Kernel* kernel, const char* name, void* pipeline)
{
    if (!kernel || !name || !pipeline)
        return -1;

    bs_mutex_lock(&kernel->registry_mu);
    if (kernel->pipeline_count >= kernel->pipeline_capacity)
    {
        if (kernel_resize_pipelines(kernel, kernel->pipeline_capacity * 2) != 0)
        {
            bs_mutex_unlock(&kernel->registry_mu);
            return -1;
        }
    }

    // Check if name already exists
    for (size_t i = 0; i < kernel->pipeline_count; i++)
    {
        if (strcmp(kernel->pipelines[i].name, name) == 0)
        {
            bs_mutex_unlock(&kernel->registry_mu);
            return -1;
        }
    }

    kernel->pipelines[kernel->pipeline_count].name        = strdup(name);
    kernel->pipelines[kernel->pipeline_count].pipeline    = pipeline;
    kernel->pipelines[kernel->pipeline_count].active_jobs = 0;
    kernel->pipelines[kernel->pipeline_count].accepting   = 1;
    kernel->pipeline_count++;

    bs_mutex_unlock(&kernel->registry_mu);
    return 0;
}

int bs_kernel_unregister_pipeline(Kernel* kernel, const char* name)
{
    if (!kernel || !name)
        return -1;

    bs_mutex_lock(&kernel->registry_mu);
    for (size_t i = 0; i < kernel->pipeline_count; i++)
    {
        if (strcmp(kernel->pipelines[i].name, name) == 0)
        {
            kernel->pipelines[i].accepting = 0;
            while (kernel->pipelines[i].active_jobs > 0)
                bs_cond_wait(&kernel->registry_cv, &kernel->registry_mu);
            free((void*)kernel->pipelines[i].name);

            for (size_t j = i; j < kernel->pipeline_count - 1; j++)
            {
                kernel->pipelines[j] = kernel->pipelines[j + 1];
            }

            kernel->pipeline_count--;
            bs_mutex_unlock(&kernel->registry_mu);
            return 0;
        }
    }

    bs_mutex_unlock(&kernel->registry_mu);
    return -1;
}

void* bs_kernel_get_pipeline(Kernel* kernel, const char* name)
{
    if (!kernel || !name)
        return NULL;

    bs_mutex_lock(&kernel->registry_mu);
    for (size_t i = 0; i < kernel->pipeline_count; i++)
    {
        if (strcmp(kernel->pipelines[i].name, name) == 0)
        {
            void* pipeline = kernel->pipelines[i].pipeline;
            bs_mutex_unlock(&kernel->registry_mu);
            return pipeline;
        }
    }

    bs_mutex_unlock(&kernel->registry_mu);
    return NULL;
}

int bs_kernel_set_config(Kernel* kernel, const KernelConfig* config)
{
    if (!kernel || !config)
        return -1;
    if (kernel->state == KERNEL_STATE_RUNNING)
        return -1;

    if (kernel->config)
    {
        bs_kernel_config_destroy(kernel->config);
    }

    kernel->config = (KernelConfig*)malloc(sizeof(KernelConfig));
    if (!kernel->config)
        return -1;

    memcpy(kernel->config, config, sizeof(KernelConfig));
    return 0;
}

const KernelConfig* bs_kernel_get_config(const Kernel* kernel)
{
    return kernel ? kernel->config : NULL;
}

const char* bs_kernel_get_version(void)
{
    return KERNEL_VERSION;
}

uint64_t bs_kernel_get_start_time(const Kernel* kernel)
{
    return kernel ? kernel->start_time : 0;
}

uint64_t bs_kernel_get_execution_count(const Kernel* kernel)
{
    return kernel ? kernel->execution_count : 0;
}
