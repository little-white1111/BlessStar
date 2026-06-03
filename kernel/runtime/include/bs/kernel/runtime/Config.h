#ifndef BS_KERNEL_RUNTIME_CONFIG_H
#define BS_KERNEL_RUNTIME_CONFIG_H

/*
 * C-ST-7 contract block:
 * Thread safety: KernelConfig is copied by value into Kernel; not shared mutable state.
 * Error semantics: bs_kernel_config_validate may return error_message alloc failures as -1.
 * Platform notes: Holds log level, timeouts, and data directory strings.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct KernelConfig KernelConfig;

    typedef enum LogLevel
    {
        LOG_LEVEL_DEBUG,
        LOG_LEVEL_INFO,
        LOG_LEVEL_WARN,
        LOG_LEVEL_ERROR
    } LogLevel;

    typedef enum ErrorHandlingMode
    {
        ERROR_HANDLING_STRICT,
        ERROR_HANDLING_RELAXED,
        ERROR_HANDLING_IGNORE
    } ErrorHandlingMode;

    struct KernelConfig
    {
        const char*       name;
        LogLevel          log_level;
        ErrorHandlingMode error_handling_mode;
        uint32_t          max_pipeline_stages;
        uint32_t          max_instruction_size;
        uint64_t          execution_timeout_ms;
        uint32_t          max_retries;
        int               enable_metrics;
        int               enable_tracing;
        int               enable_validation;
        const char*       data_dir;
        const char*       log_dir;
    };

    KernelConfig* bs_kernel_config_create(void);
    void          bs_kernel_config_destroy(KernelConfig* config);

    void        bs_kernel_config_set_name(KernelConfig* config, const char* name);
    const char* bs_kernel_config_get_name(const KernelConfig* config);

    void     bs_kernel_config_set_log_level(KernelConfig* config, LogLevel level);
    LogLevel bs_kernel_config_get_log_level(const KernelConfig* config);

    void bs_kernel_config_set_error_handling_mode(KernelConfig* config, ErrorHandlingMode mode);
    ErrorHandlingMode bs_kernel_config_get_error_handling_mode(const KernelConfig* config);

    void     bs_kernel_config_set_max_pipeline_stages(KernelConfig* config, uint32_t max);
    uint32_t bs_kernel_config_get_max_pipeline_stages(const KernelConfig* config);

    void     bs_kernel_config_set_execution_timeout_ms(KernelConfig* config, uint64_t timeout);
    uint64_t bs_kernel_config_get_execution_timeout_ms(const KernelConfig* config);

    void        bs_kernel_config_set_data_dir(KernelConfig* config, const char* dir);
    const char* bs_kernel_config_get_data_dir(const KernelConfig* config);

    int bs_kernel_config_validate(const KernelConfig* config, char** error_message);

#ifdef __cplusplus
}
#endif

#endif // BS_KERNEL_RUNTIME_CONFIG_H
