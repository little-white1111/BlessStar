#include "bs/kernel/common/bs_safe_format.h"
#include "bs/kernel/report/Result.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Result* result_create(ResultCode code, const char* message)
{
    Result* result = (Result*)malloc(sizeof(Result));
    if (!result)
        return NULL;

    result->code           = code;
    result->message        = message ? strdup(message) : NULL;
    result->detail         = NULL;
    result->retry_after_ms = 0;

    return result;
}

void result_destroy(Result* result)
{
    if (!result)
        return;

    if (result->message)
        free((void*)result->message);
    if (result->detail)
        free((void*)result->detail);

    free(result);
}

Result* result_success(void)
{
    return result_create(RESULT_SUCCESS, "Success");
}

Result* result_error(ResultCode code, const char* message)
{
    return result_create(code, message);
}

Result* result_error_with_detail(ResultCode code, const char* message, const char* detail)
{
    Result* result = result_create(code, message);
    if (result && detail)
    {
        result->detail = strdup(detail);
    }
    return result;
}

int result_is_success(const Result* result)
{
    return result && result->code == RESULT_SUCCESS;
}

int result_is_error(const Result* result)
{
    return !result_is_success(result);
}

int result_needs_retry(const Result* result)
{
    return result && result->code == RESULT_ERROR_RETRY;
}

const char* result_code_to_string(ResultCode code)
{
    switch (code)
    {
    case RESULT_SUCCESS:
        return "SUCCESS";
    case RESULT_ERROR_UNKNOWN:
        return "UNKNOWN_ERROR";
    case RESULT_ERROR_INVALID_ARGUMENT:
        return "INVALID_ARGUMENT";
    case RESULT_ERROR_NOT_FOUND:
        return "NOT_FOUND";
    case RESULT_ERROR_ALREADY_EXISTS:
        return "ALREADY_EXISTS";
    case RESULT_ERROR_TIMEOUT:
        return "TIMEOUT";
    case RESULT_ERROR_OUT_OF_MEMORY:
        return "OUT_OF_MEMORY";
    case RESULT_ERROR_IO:
        return "IO_ERROR";
    case RESULT_ERROR_PARSE:
        return "PARSE_ERROR";
    case RESULT_ERROR_VALIDATION:
        return "VALIDATION_ERROR";
    case RESULT_ERROR_EXECUTION:
        return "EXECUTION_ERROR";
    case RESULT_ERROR_DEPENDENCY:
        return "DEPENDENCY_ERROR";
    case RESULT_ERROR_VERSION_MISMATCH:
        return "VERSION_MISMATCH";
    case RESULT_ERROR_PERMISSION:
        return "PERMISSION_ERROR";
    case RESULT_ERROR_STATE_CONFLICT:
        return "STATE_CONFLICT";
    case RESULT_ERROR_RETRY:
        return "RETRY";
    case RESULT_ERROR_CANCELLED:
        return "CANCELLED";
    default:
        return "UNKNOWN";
    }
}

char* result_to_string(const Result* result)
{
    if (!result)
        return NULL;

    const char* code_str = result_code_to_string(result->code);
    size_t      size     = 512;
    char*       output   = (char*)malloc(size);

    if (!output)
        return NULL;

    if (result->detail)
    {
        bs_safe_snprintf(output, size, "%s: %s - %s", code_str, result->message, result->detail);
    }
    else if (result->message)
    {
        bs_safe_snprintf(output, size, "%s: %s", code_str, result->message);
    }
    else
    {
        bs_safe_snprintf(output, size, "%s", code_str);
    }

    return output;
}

char* result_to_json(const Result* result)
{
    if (!result)
        return NULL;

    const char* code_str = result_code_to_string(result->code);
    size_t      size     = 512;
    char*       output   = (char*)malloc(size);

    if (!output)
        return NULL;

    if (result->detail)
    {
        bs_safe_snprintf(
            output, size,
            "{\"code\":%d,\"code_str\":\"%s\",\"message\":\"%s\",\"detail\":\"%s\",\"retry_"
            "after_ms\":%llu}",
            (int)result->code, code_str, result->message ? result->message : "", result->detail,
            (unsigned long long)result->retry_after_ms);
    }
    else if (result->message)
    {
        bs_safe_snprintf(
            output, size,
            "{\"code\":%d,\"code_str\":\"%s\",\"message\":\"%s\",\"retry_after_ms\":%llu}",
            (int)result->code, code_str, result->message,
            (unsigned long long)result->retry_after_ms);
    }
    else
    {
        bs_safe_snprintf(output, size, "{\"code\":%d,\"code_str\":\"%s\",\"retry_after_ms\":%llu}",
                         (int)result->code, code_str, (unsigned long long)result->retry_after_ms);
    }

    return output;
}
