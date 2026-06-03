#ifndef BS_KERNEL_REGISTRY_REGISTRY_STATUS_TABLE_H
#define BS_KERNEL_REGISTRY_REGISTRY_STATUS_TABLE_H

/*
 * C-ST-7 contract block:
 * Thread safety: Read-only status strings; reentrant.
 * Error semantics: Maps registry error integers to text for logs/reports.
 * Platform notes: Companion to registry_facade error returns.
 */

#include "bs/kernel/common/bs_status.h"
#include "bs/kernel/registry/types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    extern const BsStatusCodeEntry k_registry_status_table[];
    extern const size_t            k_registry_status_table_len;

    /** After `register_status_domain` for `"registry"`, pass `out_domain_id` (IMPL-08-13). */
    void bs_registry_status_set_domain_id(uint16_t domain_id);

    BsStatus bs_status_from_registry(int registry_status);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_REGISTRY_REGISTRY_STATUS_TABLE_H */
