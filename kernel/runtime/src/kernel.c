#include "bs/kernel/ir/ir.h"
#include "bs/kernel/pipeline/pipeline.h"
#include "bs/kernel/report/report.h"
#include "bs/kernel/runtime/Config.h"
#include "bs/kernel/runtime/Context.h"
#include "bs/kernel/runtime/Kernel.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PIPELINE_INITIAL_CAPACITY 4
#define KERNEL_ASYNC_QUEUE_CAP 32

typedef struct KernelAsyncNode
{
    IRInstruction*          ir;
    struct KernelAsyncNode* next;
} KernelAsyncNode;

typedef struct PipelineEntry
{
    const char* name;
    void*       pipeline;
} PipelineEntry;

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
};

static const char* KERNEL_VERSION = "1.0.0";

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

    return kernel;
}

void bs_kernel_destroy(Kernel* kernel)
{
    if (!kernel)
        return;

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

    // Cleanup components
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

    Pipeline* pipeline = (Pipeline*)bs_kernel_get_pipeline(kernel, "default");
    if (!pipeline && kernel->pipeline_count > 0)
        pipeline = (Pipeline*)kernel->pipelines[0].pipeline;

    if (!pipeline)
        return NULL;

    Report* pipe_report = NULL;
    if (bs_pipeline_execute(pipeline, ir, &pipe_report) != 0)
    {
        if (pipe_report)
            bs_report_destroy(pipe_report);
        return NULL;
    }

    kernel->execution_count++;
    return pipe_report;
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

    if (kernel->pipeline_count >= kernel->pipeline_capacity)
    {
        if (kernel_resize_pipelines(kernel, kernel->pipeline_capacity * 2) != 0)
        {
            return -1;
        }
    }

    // Check if name already exists
    for (size_t i = 0; i < kernel->pipeline_count; i++)
    {
        if (strcmp(kernel->pipelines[i].name, name) == 0)
        {
            return -1;
        }
    }

    kernel->pipelines[kernel->pipeline_count].name     = strdup(name);
    kernel->pipelines[kernel->pipeline_count].pipeline = pipeline;
    kernel->pipeline_count++;

    return 0;
}

int bs_kernel_unregister_pipeline(Kernel* kernel, const char* name)
{
    if (!kernel || !name)
        return -1;

    for (size_t i = 0; i < kernel->pipeline_count; i++)
    {
        if (strcmp(kernel->pipelines[i].name, name) == 0)
        {
            free((void*)kernel->pipelines[i].name);

            for (size_t j = i; j < kernel->pipeline_count - 1; j++)
            {
                kernel->pipelines[j] = kernel->pipelines[j + 1];
            }

            kernel->pipeline_count--;
            return 0;
        }
    }

    return -1;
}

void* bs_kernel_get_pipeline(Kernel* kernel, const char* name)
{
    if (!kernel || !name)
        return NULL;

    for (size_t i = 0; i < kernel->pipeline_count; i++)
    {
        if (strcmp(kernel->pipelines[i].name, name) == 0)
        {
            return kernel->pipelines[i].pipeline;
        }
    }

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
