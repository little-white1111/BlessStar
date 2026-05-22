#include "bs/kernel/common/bs_status.h"
#include "bs/kernel/report/Result.h"

#include <cassert>

int main()
{
    constexpr uint16_t kAnyDomain = 42;
    assert(bs_result_code_from_bs_status(BS_STATUS_OK) == RESULT_SUCCESS);
    assert(bs_result_code_from_bs_status(bs_status_make(kAnyDomain, 5)) == RESULT_ERROR_TIMEOUT);
    assert(bs_result_code_from_bs_status(bs_status_make(kAnyDomain, 6)) == RESULT_ERROR_NOT_FOUND);
    assert(bs_result_code_from_bs_status(bs_status_make(kAnyDomain, 7)) ==
           RESULT_ERROR_INVALID_ARGUMENT);
    assert(bs_result_code_from_bs_status(bs_status_make(kAnyDomain, 3)) == RESULT_ERROR_IO);
    return 0;
}
