#ifndef BS_ADAPTER_PLUGIN_PLUGIN_API_H
#define BS_ADAPTER_PLUGIN_PLUGIN_API_H

#include "bs/kernel/registry/registry_facade.h"

#include "bs/adapter/attach_context.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** C ABI plugin entry (PLUGIN-VIII · ADR-BS-ABI-001). */
    typedef int (*BsAdapterPluginRegisterFn)(RegistryFacade* facade, AttachContext* ctx);

    int bs_adapter_plugin_io_register(RegistryFacade* facade, AttachContext* ctx);
    int bs_adapter_plugin_log_domains_register(RegistryFacade* facade, AttachContext* ctx);
    int bs_adapter_plugin_orch_register(RegistryFacade* facade, AttachContext* ctx);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PLUGIN_PLUGIN_API_H */
