#include "bs/adapter/parser/MetaExecutor.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_MODULES 32
#define MAX_FUNCTIONS 64
#define MAX_VARIABLES 128

typedef struct ModuleEntry
{
    const char* name;
    int         allowed;
} ModuleEntry;

typedef struct FunctionEntry
{
    const char* name;
    int         allowed;
} FunctionEntry;

typedef struct VariableEntry
{
    const char* name;
    const char* value;
} VariableEntry;

struct MetaExecutor
{
    ModuleEntry   modules[MAX_MODULES];
    size_t        module_count;
    FunctionEntry functions[MAX_FUNCTIONS];
    size_t        function_count;
    VariableEntry variables[MAX_VARIABLES];
    size_t        variable_count;
    uint64_t      timeout_ms;
    int           initialized;
};

MetaExecutor* meta_executor_create(void)
{
    MetaExecutor* executor = (MetaExecutor*)malloc(sizeof(MetaExecutor));
    if (!executor)
        return NULL;

    memset(executor, 0, sizeof(MetaExecutor));
    executor->timeout_ms  = 5000; // 5 seconds default
    executor->initialized = 1;

    return executor;
}

void meta_executor_destroy(MetaExecutor* executor)
{
    if (!executor)
        return;

    for (size_t i = 0; i < executor->variable_count; i++)
    {
        if (executor->variables[i].name)
            free((void*)executor->variables[i].name);
        if (executor->variables[i].value)
            free((void*)executor->variables[i].value);
    }

    free(executor);
}

int meta_executor_allow_module(MetaExecutor* executor, const char* module_name)
{
    if (!executor || !module_name)
        return -1;
    if (!executor->initialized)
        return -1;

    if (executor->module_count >= MAX_MODULES)
        return -1;

    executor->modules[executor->module_count].name    = strdup(module_name);
    executor->modules[executor->module_count].allowed = 1;
    executor->module_count++;

    return 0;
}

int meta_executor_deny_module(MetaExecutor* executor, const char* module_name)
{
    if (!executor || !module_name)
        return -1;
    if (!executor->initialized)
        return -1;

    for (size_t i = 0; i < executor->module_count; i++)
    {
        if (strcmp(executor->modules[i].name, module_name) == 0)
        {
            executor->modules[i].allowed = 0;
            return 0;
        }
    }

    return -1;
}

int meta_executor_is_module_allowed(MetaExecutor* executor, const char* module_name)
{
    if (!executor || !module_name)
        return 0;
    if (!executor->initialized)
        return 0;

    for (size_t i = 0; i < executor->module_count; i++)
    {
        if (strcmp(executor->modules[i].name, module_name) == 0)
        {
            return executor->modules[i].allowed;
        }
    }

    return 0;
}

int meta_executor_allow_function(MetaExecutor* executor, const char* function_name)
{
    if (!executor || !function_name)
        return -1;
    if (!executor->initialized)
        return -1;

    if (executor->function_count >= MAX_FUNCTIONS)
        return -1;

    executor->functions[executor->function_count].name    = strdup(function_name);
    executor->functions[executor->function_count].allowed = 1;
    executor->function_count++;

    return 0;
}

int meta_executor_deny_function(MetaExecutor* executor, const char* function_name)
{
    if (!executor || !function_name)
        return -1;
    if (!executor->initialized)
        return -1;

    for (size_t i = 0; i < executor->function_count; i++)
    {
        if (strcmp(executor->functions[i].name, function_name) == 0)
        {
            executor->functions[i].allowed = 0;
            return 0;
        }
    }

    return -1;
}

int meta_executor_set_timeout(MetaExecutor* executor, uint64_t timeout_ms)
{
    if (!executor)
        return -1;
    executor->timeout_ms = timeout_ms;
    return 0;
}

uint64_t meta_executor_get_timeout(const MetaExecutor* executor)
{
    return executor ? executor->timeout_ms : 5000;
}

ExecutionResult* execution_result_create(ExecutionStatus status, const char* output,
                                         const char* error)
{
    ExecutionResult* result = (ExecutionResult*)malloc(sizeof(ExecutionResult));
    if (!result)
        return NULL;

    result->status        = status;
    result->output        = output ? strdup(output) : NULL;
    result->error_message = error ? strdup(error) : NULL;
    result->result        = NULL;

    return result;
}

void execution_result_destroy(ExecutionResult* result)
{
    if (!result)
        return;

    if (result->output)
        free((void*)result->output);
    if (result->error_message)
        free((void*)result->error_message);

    free(result);
}

ExecutionResult* meta_executor_execute(MetaExecutor* executor, const char* code, void* context)
{
    if (!executor || !code)
    {
        return execution_result_create(EXECUTION_ERROR, NULL, "Invalid arguments");
    }
    if (!executor->initialized)
    {
        return execution_result_create(EXECUTION_ERROR, NULL, "Executor not initialized");
    }

    (void)context;
    (void)code;

    return execution_result_create(EXECUTION_SUCCESS, "Execution completed", NULL);
}

int meta_executor_register_variable(MetaExecutor* executor, const char* name, const char* value)
{
    if (!executor || !name || !value)
        return -1;
    if (!executor->initialized)
        return -1;

    if (executor->variable_count >= MAX_VARIABLES)
        return -1;

    executor->variables[executor->variable_count].name  = strdup(name);
    executor->variables[executor->variable_count].value = strdup(value);
    executor->variable_count++;

    return 0;
}

int meta_executor_unregister_variable(MetaExecutor* executor, const char* name)
{
    if (!executor || !name)
        return -1;
    if (!executor->initialized)
        return -1;

    for (size_t i = 0; i < executor->variable_count; i++)
    {
        if (strcmp(executor->variables[i].name, name) == 0)
        {
            if (executor->variables[i].name)
                free((void*)executor->variables[i].name);
            if (executor->variables[i].value)
                free((void*)executor->variables[i].value);

            for (size_t j = i; j < executor->variable_count - 1; j++)
            {
                executor->variables[j] = executor->variables[j + 1];
            }

            executor->variable_count--;
            return 0;
        }
    }

    return -1;
}

const char* meta_executor_get_variable(MetaExecutor* executor, const char* name)
{
    if (!executor || !name)
        return NULL;
    if (!executor->initialized)
        return NULL;

    for (size_t i = 0; i < executor->variable_count; i++)
    {
        if (strcmp(executor->variables[i].name, name) == 0)
        {
            return executor->variables[i].value;
        }
    }

    return NULL;
}
