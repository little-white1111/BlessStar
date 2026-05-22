#include "bs/kernel/ir/ir.h"
#include "bs/kernel/report/report.h"
#include "bs/kernel/runtime/Config.h"
#include "bs/kernel/runtime/Context.h"
#include "bs/kernel/runtime/Kernel.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PIPELINE_INITIAL_CAPACITY 4

typedef struct PipelineEntry
{
    const char* name;
    void*       pipeline;
} PipelineEntry;

struct Kernel
{
    KernelState    state;
    KernelConfig*  config;
    Context*       context;
    PipelineEntry* pipelines;
    size_t         pipeline_count;
    size_t         pipeline_capacity;
    uint64_t       start_time;
    uint64_t       execution_count;
};

static const char* KERNEL_VERSION = "1.0.0";

Kernel* kernel_create(const KernelConfig* config)
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
        kernel->config = kernel_config_create();
        if (!kernel->config)
        {
            free(kernel);
            return NULL;
        }
    }

    kernel->context = context_create(CONTEXT_SCOPE_GLOBAL);
    if (!kernel->context)
    {
        if (kernel->config)
            kernel_config_destroy(kernel->config);
        free(kernel);
        return NULL;
    }

    kernel->pipelines = (PipelineEntry*)malloc(sizeof(PipelineEntry) * PIPELINE_INITIAL_CAPACITY);
    if (!kernel->pipelines)
    {
        context_destroy(kernel->context);
        if (kernel->config)
            kernel_config_destroy(kernel->config);
        free(kernel);
        return NULL;
    }

    kernel->pipeline_count    = 0;
    kernel->pipeline_capacity = PIPELINE_INITIAL_CAPACITY;
    kernel->start_time        = 0;
    kernel->execution_count   = 0;

    return kernel;
}

void kernel_destroy(Kernel* kernel)
{
    if (!kernel)
        return;

    for (size_t i = 0; i < kernel->pipeline_count; i++)
    {
        if (kernel->pipelines[i].name)
        {
            free((void*)kernel->pipelines[i].name);
        }
        // Assume pipeline has its own destroy function
        // pipeline_destroy(kernel->pipelines[i].pipeline);
    }

    if (kernel->pipelines)
        free(kernel->pipelines);
    if (kernel->context)
        context_destroy(kernel->context);
    if (kernel->config)
        kernel_config_destroy(kernel->config);

    free(kernel);
}

int kernel_start(Kernel* kernel)
{
    if (!kernel)
        return -1;
    if (kernel->state != KERNEL_STATE_STOPPED)
        return -1;

    kernel->state = KERNEL_STATE_STARTING;

    // Initialize components
    context_set_metadata(kernel->context, "kernel.version", KERNEL_VERSION);

    kernel->start_time = (uint64_t)time(NULL);
    kernel->state      = KERNEL_STATE_RUNNING;

    return 0;
}

int kernel_stop(Kernel* kernel)
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

KernelState kernel_get_state(const Kernel* kernel)
{
    return kernel ? kernel->state : KERNEL_STATE_ERROR;
}

Report* kernel_execute(Kernel* kernel, const IRInstruction* ir)
{
    if (!kernel || !ir)
        return NULL;
    if (kernel->state != KERNEL_STATE_RUNNING)
        return NULL;

    Report* report = report_create("kernel_execution");
    if (!report)
        return NULL;

    report_mark_start(report);
    report_add_info(report, "kernel", "Starting execution");

    // Execute through registered pipelines
    for (size_t i = 0; i < kernel->pipeline_count; i++)
    {
        report_add_info(report, "kernel", "Executing pipeline: ");
        // Incomplete - would call pipeline_execute here
    }

    kernel->execution_count++;

    report_set_status(report, REPORT_STATUS_SUCCESS);
    report_add_info(report, "kernel", "Execution completed");
    report_mark_end(report);

    return report;
}

int kernel_execute_async(Kernel* kernel, const IRInstruction* ir)
{
    if (!kernel || !ir)
        return -1;
    if (kernel->state != KERNEL_STATE_RUNNING)
        return -1;

    // In async implementation, would queue for background execution
    // For now, just execute synchronously
    Report* report = kernel_execute(kernel, ir);
    if (report)
    {
        report_destroy(report);
        return 0;
    }

    return -1;
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

int kernel_register_pipeline(Kernel* kernel, const char* name, void* pipeline)
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

int kernel_unregister_pipeline(Kernel* kernel, const char* name)
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

void* kernel_get_pipeline(Kernel* kernel, const char* name)
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

int kernel_set_config(Kernel* kernel, const KernelConfig* config)
{
    if (!kernel || !config)
        return -1;
    if (kernel->state == KERNEL_STATE_RUNNING)
        return -1;

    if (kernel->config)
    {
        kernel_config_destroy(kernel->config);
    }

    kernel->config = (KernelConfig*)malloc(sizeof(KernelConfig));
    if (!kernel->config)
        return -1;

    memcpy(kernel->config, config, sizeof(KernelConfig));
    return 0;
}

const KernelConfig* kernel_get_config(const Kernel* kernel)
{
    return kernel ? kernel->config : NULL;
}

const char* kernel_get_version(void)
{
    return KERNEL_VERSION;
}

uint64_t kernel_get_start_time(const Kernel* kernel)
{
    return kernel ? kernel->start_time : 0;
}

uint64_t kernel_get_execution_count(const Kernel* kernel)
{
    return kernel ? kernel->execution_count : 0;
}
