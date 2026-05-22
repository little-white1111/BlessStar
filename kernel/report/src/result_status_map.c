#include "bs/kernel/common/bs_status.h"
#include "bs/kernel/report/Result.h"

ResultCode bs_result_code_from_bs_status(BsStatus status)
{
    if (bs_status_is_ok(status))
        return RESULT_SUCCESS;

    const int code = bs_status_code(status);
    switch (code)
    {
    case 5:
        return RESULT_ERROR_TIMEOUT;
    case 6:
        return RESULT_ERROR_NOT_FOUND;
    case 7:
        return RESULT_ERROR_INVALID_ARGUMENT;
    default:
        return RESULT_ERROR_IO;
    }
}
