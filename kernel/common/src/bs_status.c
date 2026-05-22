#include "bs/kernel/common/bs_status.h"

BsStatus bs_status_make(uint16_t domain_id, int code)
{
    if (code == 0)
        return BS_STATUS_OK;
    const int encoded = domain_id * BS_STATUS_DOMAIN_ENCODE_K + code;
    return -encoded;
}

int bs_status_domain_id(BsStatus status)
{
    if (status >= 0)
        return 0;
    return (-status) / BS_STATUS_DOMAIN_ENCODE_K;
}

int bs_status_code(BsStatus status)
{
    if (status >= 0)
        return 0;
    return (-status) % BS_STATUS_DOMAIN_ENCODE_K;
}

int bs_status_is_ok(BsStatus status)
{
    return status == BS_STATUS_OK;
}
