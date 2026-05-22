#ifndef BS_ADAPTER_IO_PROVIDER_STUBS_H
#define BS_ADAPTER_IO_PROVIDER_STUBS_H

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
