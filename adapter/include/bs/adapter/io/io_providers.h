#ifndef BS_ADAPTER_IO_IO_PROVIDERS_H
#define BS_ADAPTER_IO_IO_PROVIDERS_H

/*
 * C-ST-7 contract block:
 * Thread safety: Provider table mutations only during bootstrap.
 * Error semantics: Registration failures return BsStatus to bootstrap caller.
 * Platform notes: Registers local/stub/remote providers with registry hub.
 */

#include "bs/kernel/registry/registry_facade.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** Register /adapter/io/{local,db,remote} declarations, hub mappings, and bindings (P2,
     * pre-freeze). */
    int bs_adapter_io_register_providers(RegistryFacade* facade);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_IO_IO_PROVIDERS_H */
