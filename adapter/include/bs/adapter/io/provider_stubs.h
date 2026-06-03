#ifndef BS_ADAPTER_IO_PROVIDER_STUBS_H
#define BS_ADAPTER_IO_PROVIDER_STUBS_H

/*
 * C-ST-7 contract block:
 * Thread safety: Stubs are stateless aside from configured failure injection.
 * Error semantics: Deterministic BS_IO_ERR_* for tests.
 * Platform notes: Used when remote/DB providers are not linked.
 */

#include "bs/kernel/io/io.h"

#ifdef __cplusplus
extern "C"
{
#endif

    IoProviderBinding* bs_adapter_io_db_stub_binding(void);
    IoProviderBinding* bs_adapter_io_remote_stub_binding(void);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_IO_PROVIDER_STUBS_H */
