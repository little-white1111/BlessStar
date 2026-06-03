#ifndef BS_ADAPTER_IO_LOCAL_FILE_PROVIDER_H
#define BS_ADAPTER_IO_LOCAL_FILE_PROVIDER_H

/*
 * C-ST-7 contract block:
 * Thread safety: Provider instance not thread-safe; one facade serializes calls.
 * Error semantics: BS_IO_* via IoReadResult; max read limits enforced.
 * Platform notes: file:// URI reads for local config paths.
 */

#include "bs/kernel/io/io.h"
#include "bs/kernel/registry/registry_facade.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct LocalFileProvider LocalFileProvider;

    LocalFileProvider* bs_adapter_io_local_provider_create(void);
    void               bs_adapter_io_local_provider_destroy(LocalFileProvider* provider);

    IoProviderBinding* bs_adapter_io_local_provider_binding(LocalFileProvider* provider);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_IO_LOCAL_FILE_PROVIDER_H */
