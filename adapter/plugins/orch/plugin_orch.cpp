#include "bs/adapter/orchestration/reload_batch_factory.h"
#include "bs/adapter/plugin/plugin_api.h"
#include "bs/adapter/plugin/plugin_ir_requirements.h"
#include "bs/adapter/plugin/plugin_manifest_paths.h"

int bs_adapter_plugin_orch_register(RegistryFacade* facade, AttachContext* ctx)
{
    (void)ctx;

    char path[512];
    if (bs_adapter_plugin_manifest_path("ir_requirements_orch.txt", path, sizeof(path)) == 0)
    {
        if (bs_adapter_plugin_validate_ir_requirements_ref(path) != 0)
            return -1;
    }

    return bs_adapter_reload_batch_register_factory(facade);
}
