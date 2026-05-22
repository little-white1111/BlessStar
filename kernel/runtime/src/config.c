#include "bs/kernel/runtime/Config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

KernelConfig* kernel_config_create(void)
{
    KernelConfig* config = (KernelConfig*)malloc(sizeof(KernelConfig));
    if (!config)
        return NULL;

    config->name                 = strdup("default");
    config->log_level            = LOG_LEVEL_INFO;
    config->error_handling_mode  = ERROR_HANDLING_STRICT;
    config->max_pipeline_stages  = 32;
    config->max_instruction_size = 1024 * 1024; // 1MB
    config->execution_timeout_ms = 30000;       // 30 seconds
    config->max_retries          = 3;
    config->enable_metrics       = 1;
    config->enable_tracing       = 0;
    config->enable_validation    = 1;
    config->data_dir             = strdup(".");
    config->log_dir              = strdup(".");

    return config;
}

void kernel_config_destroy(KernelConfig* config)
{
    if (!config)
        return;

    if (config->name)
        free((void*)config->name);
    if (config->data_dir)
        free((void*)config->data_dir);
    if (config->log_dir)
        free((void*)config->log_dir);

    free(config);
}

void kernel_config_set_name(KernelConfig* config, const char* name)
{
    if (!config || !name)
        return;
    if (config->name)
        free((void*)config->name);
    config->name = strdup(name);
}

const char* kernel_config_get_name(const KernelConfig* config)
{
    return config ? config->name : NULL;
}

void kernel_config_set_log_level(KernelConfig* config, LogLevel level)
{
    if (!config)
        return;
    config->log_level = level;
}

LogLevel kernel_config_get_log_level(const KernelConfig* config)
{
    return config ? config->log_level : LOG_LEVEL_INFO;
}

void kernel_config_set_error_handling_mode(KernelConfig* config, ErrorHandlingMode mode)
{
    if (!config)
        return;
    config->error_handling_mode = mode;
}

ErrorHandlingMode kernel_config_get_error_handling_mode(const KernelConfig* config)
{
    return config ? config->error_handling_mode : ERROR_HANDLING_STRICT;
}

void kernel_config_set_max_pipeline_stages(KernelConfig* config, uint32_t max)
{
    if (!config)
        return;
    config->max_pipeline_stages = max;
}

uint32_t kernel_config_get_max_pipeline_stages(const KernelConfig* config)
{
    return config ? config->max_pipeline_stages : 32;
}

void kernel_config_set_execution_timeout_ms(KernelConfig* config, uint64_t timeout)
{
    if (!config)
        return;
    config->execution_timeout_ms = timeout;
}

uint64_t kernel_config_get_execution_timeout_ms(const KernelConfig* config)
{
    return config ? config->execution_timeout_ms : 30000;
}

void kernel_config_set_data_dir(KernelConfig* config, const char* dir)
{
    if (!config || !dir)
        return;
    if (config->data_dir)
        free((void*)config->data_dir);
    config->data_dir = strdup(dir);
}

const char* kernel_config_get_data_dir(const KernelConfig* config)
{
    return config ? config->data_dir : NULL;
}

int kernel_config_validate(const KernelConfig* config, char** error_message)
{
    if (!config)
    {
        if (error_message)
        {
            *error_message = strdup("Config is NULL");
        }
        return -1;
    }

    if (!config->name || strlen(config->name) == 0)
    {
        if (error_message)
        {
            *error_message = strdup("Config name is empty");
        }
        return -1;
    }

    if (config->max_pipeline_stages == 0)
    {
        if (error_message)
        {
            *error_message = strdup("Max pipeline stages must be > 0");
        }
        return -1;
    }

    if (config->max_instruction_size == 0)
    {
        if (error_message)
        {
            *error_message = strdup("Max instruction size must be > 0");
        }
        return -1;
    }

    if (!config->data_dir)
    {
        if (error_message)
        {
            *error_message = strdup("Data directory is NULL");
        }
        return -1;
    }

    if (!config->log_dir)
    {
        if (error_message)
        {
            *error_message = strdup("Log directory is NULL");
        }
        return -1;
    }

    return 0;
}
