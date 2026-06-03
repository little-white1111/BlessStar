#ifndef BS_ADAPTER_ORCHESTRATION_RELOAD_BATCH_FACTORY_H
#define BS_ADAPTER_ORCHESTRATION_RELOAD_BATCH_FACTORY_H

/*
 * C-ST-7 contract block:
 * Thread safety: Factory builds isolated controller instances; not shared.
 * Error semantics: NULL controller when allocation or attach_store binding fails.
 * Platform notes: Convenience ctor for tests and adapter_cli.
 */

#include "bs/kernel/registry/registry_facade.h"
#include "bs/kernel/registry/types.h"

#include "bs/adapter/orchestration/reload_batch_controller.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct ReloadBatchControllerFactory ReloadBatchControllerFactory;

    typedef ReloadBatchController* (*ReloadBatchFactoryCreateFn)(unsigned max_inflight,
                                                                 void*    user_ctx);
    typedef void (*ReloadBatchFactoryDestroyFn)(ReloadBatchController* ctrl, void* user_ctx);

    struct ReloadBatchControllerFactory
    {
        ReloadBatchFactoryCreateFn  create;
        ReloadBatchFactoryDestroyFn destroy;
        void*                       user_ctx;
    };

    const ReloadBatchControllerFactory* bs_adapter_attach_reload_batch_default_factory(void);

    /** Register /adapter/orchestration/reload_batch (type_constraint reload_batch). */
    int bs_adapter_attach_reload_batch_register_factory(RegistryFacade* facade);

    ReloadBatchController*
    bs_adapter_attach_reload_batch_create_from_binding(const Binding* binding,
                                                       unsigned       max_inflight);

    void bs_adapter_attach_reload_batch_destroy_from_binding(const Binding*         binding,
                                                             ReloadBatchController* ctrl);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_ORCHESTRATION_RELOAD_BATCH_FACTORY_H */
