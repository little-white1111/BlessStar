#include "bs/kernel/io/io.h"
#include "bs/kernel/io/io_status_table.h"

const BsStatusCodeEntry k_io_status_table[] = {
    {0, "OK", 0},          {1, "INVALID_URI", 0}, {2, "UNSUPPORTED_SCHEME", 0},
    {3, "PROVIDER", 0},    {4, "READ_LIMIT", 0},  {5, "TIMEOUT", 0},
    {6, "NOT_FOUND", 0},   {7, "INVALID_ARG", 0}, {8, "REGISTRY", 0},
    {9, "NO_PROVIDER", 0},
};

const size_t k_io_status_table_len = sizeof(k_io_status_table) / sizeof(k_io_status_table[0]);

static uint16_t g_io_status_domain_id = 1;

void bs_io_status_set_domain_id(uint16_t domain_id)
{
    g_io_status_domain_id = domain_id;
}

static int io_status_to_code(int io_status)
{
    switch (io_status)
    {
    case BS_IO_OK:
        return 0;
    case BS_IO_ERR_INVALID_URI:
        return 1;
    case BS_IO_ERR_UNSUPPORTED_SCHEME:
        return 2;
    case BS_IO_ERR_PROVIDER:
        return 3;
    case BS_IO_ERR_READ_LIMIT:
        return 4;
    case BS_IO_ERR_TIMEOUT:
        return 5;
    case BS_IO_ERR_NOT_FOUND:
        return 6;
    case BS_IO_ERR_INVALID_ARG:
        return 7;
    case BS_IO_ERR_REGISTRY:
        return 8;
    case BS_IO_ERR_NO_PROVIDER:
        return 9;
    default:
        return 3;
    }
}

BsStatus bs_status_from_io(int io_status)
{
    if (io_status == BS_IO_OK)
        return BS_STATUS_OK;
    const int code = io_status_to_code(io_status);
    return bs_status_make(g_io_status_domain_id, code);
}
