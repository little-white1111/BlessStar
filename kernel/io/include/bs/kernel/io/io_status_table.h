#ifndef BS_KERNEL_IO_IO_STATUS_TABLE_H
#define BS_KERNEL_IO_IO_STATUS_TABLE_H

#include "bs/kernel/common/bs_status.h"

#ifdef __cplusplus
extern "C"
{
#endif

    extern const BsStatusCodeEntry k_io_status_table[];
    extern const size_t            k_io_status_table_len;

    /** After `register_status_domain` for `"io"`, pass the assigned `out_domain_id` (IMPL-08-13). */
    void bs_io_status_set_domain_id(uint16_t domain_id);

    BsStatus bs_status_from_io(int io_status);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_IO_IO_STATUS_TABLE_H */
