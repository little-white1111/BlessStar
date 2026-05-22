#ifndef BS_ADAPTER_PARSER_META_EXECUTOR_H
#define BS_ADAPTER_PARSER_META_EXECUTOR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct MetaExecutor    MetaExecutor;
    typedef struct ExecutionResult ExecutionResult;

    typedef enum ExecutionStatus
    {
        EXECUTION_SUCCESS,
        EXECUTION_ERROR,
        EXECUTION_TIMEOUT,
        EXECUTION_SECURITY_VIOLATION
    } ExecutionStatus;

    struct ExecutionResult
    {
        ExecutionStatus status;
        const char*     output;
        const char*     error_message;
        void*           result;
    };

    MetaExecutor* meta_executor_create(void);
    void          meta_executor_destroy(MetaExecutor* executor);

    int meta_executor_allow_module(MetaExecutor* executor, const char* module_name);
    int meta_executor_deny_module(MetaExecutor* executor, const char* module_name);
    int meta_executor_is_module_allowed(MetaExecutor* executor, const char* module_name);

    int meta_executor_allow_function(MetaExecutor* executor, const char* function_name);
    int meta_executor_deny_function(MetaExecutor* executor, const char* function_name);

    int      meta_executor_set_timeout(MetaExecutor* executor, uint64_t timeout_ms);
    uint64_t meta_executor_get_timeout(const MetaExecutor* executor);

    ExecutionResult* meta_executor_execute(MetaExecutor* executor, const char* code, void* context);
    void             execution_result_destroy(ExecutionResult* result);

    int         meta_executor_register_variable(MetaExecutor* executor, const char* name,
                                                const char* value);
    int         meta_executor_unregister_variable(MetaExecutor* executor, const char* name);
    const char* meta_executor_get_variable(MetaExecutor* executor, const char* name);

#ifdef __cplusplus
}
#endif

#endif // BS_ADAPTER_PARSER_META_EXECUTOR_H
