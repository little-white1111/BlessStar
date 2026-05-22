#ifndef BS_ADAPTER_ORCHESTRATION_RELOAD_BATCH_FACTORY_H
#define BS_ADAPTER_ORCHESTRATION_RELOAD_BATCH_FACTORY_H

#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/kernel/registry/registry_facade.h"
#include "bs/kernel/registry/types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct ReloadBatchControllerFactory ReloadBatchControllerFactory;

    typedef ReloadBatchController* (*ReloadBatchFactoryCreateFn)(unsigned max_inflight,
                                                                 void* user_ctx);
    typedef void (*ReloadBatchFactoryDestroyFn)(ReloadBatchController* ctrl, void* user_ctx);

    struct ReloadBatchControllerFactory
    {
        ReloadBatchFactoryCreateFn create;
        ReloadBatchFactoryDestroyFn destroy;
        void* user_ctx;
    };

    const ReloadBatchControllerFactory* bs_adapter_reload_batch_default_factory(void);

    /** Register /adapter/orchestration/reload_batch (type_constraint reload_batch). */
    int bs_adapter_reload_batch_register_factory(RegistryFacade* facade);

    ReloadBatchController* bs_reload_batch_controller_create_from_binding(const Binding* binding,
                                                                          unsigned max_inflight);

    void bs_reload_batch_controller_destroy_from_binding(const Binding* binding,
                                                         ReloadBatchController* ctrl);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_ORCHESTRATION_RELOAD_BATCH_FACTORY_H */
