#ifndef BS_KERNEL_REPORT_RESULT_H
#define BS_KERNEL_REPORT_RESULT_H

/*
 * C-ST-7 contract block:
 * Thread safety: Result objects are independent heap nodes; not shared across threads.
 * Error semantics: Factory helpers return NULL on OOM; bs_result_code_from_bs_status maps domains.
 * Platform notes: Rich single-point errors complement thin BsStatus on IO paths.
 */

#include "bs/kernel/common/bs_status.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum ResultCode
    {
        RESULT_SUCCESS                = 0,
        RESULT_ERROR_UNKNOWN          = 1,
        RESULT_ERROR_INVALID_ARGUMENT = 2,
        RESULT_ERROR_NOT_FOUND        = 3,
        RESULT_ERROR_ALREADY_EXISTS   = 4,
        RESULT_ERROR_TIMEOUT          = 5,
        RESULT_ERROR_OUT_OF_MEMORY    = 6,
        RESULT_ERROR_IO               = 7,
        RESULT_ERROR_PARSE            = 8,
        RESULT_ERROR_VALIDATION       = 9,
        RESULT_ERROR_EXECUTION        = 10,
        RESULT_ERROR_DEPENDENCY       = 11,
        RESULT_ERROR_VERSION_MISMATCH = 12,
        RESULT_ERROR_PERMISSION       = 13,
        RESULT_ERROR_STATE_CONFLICT   = 14,
        RESULT_ERROR_RETRY            = 15,
        RESULT_ERROR_CANCELLED        = 16
    } ResultCode;

    typedef struct Result Result;

    struct Result
    {
        ResultCode  code;
        const char* message;
        const char* detail;
        uint64_t    retry_after_ms;
    };

    Result* bs_result_create(ResultCode code, const char* message);
    void    bs_result_destroy(Result* result);

    Result* bs_result_success(void);
    Result* bs_result_error(ResultCode code, const char* message);
    Result* bs_result_error_with_detail(ResultCode code, const char* message, const char* detail);

    int bs_result_is_success(const Result* result);
    int bs_result_is_error(const Result* result);
    int bs_result_needs_retry(const Result* result);

    const char* bs_result_code_to_string(ResultCode code);
    char*       bs_result_to_string(const Result* result);
    char*       bs_result_to_json(const Result* result);

    ResultCode bs_result_code_from_bs_status(BsStatus status);

#ifdef __cplusplus
}
#endif

#endif // BS_KERNEL_REPORT_RESULT_H
