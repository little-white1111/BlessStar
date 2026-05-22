#include "bs/adapter/io/io_providers.h"
#include "bs/adapter/plugin/plugin_api.h"
#include "bs/adapter/plugin/plugin_ir_requirements.h"
#include "bs/adapter/plugin/plugin_manifest_paths.h"

int bs_adapter_plugin_io_register(RegistryFacade* facade, AttachContext* ctx)
{
    (void)ctx;

    char path[512];
    if (bs_adapter_plugin_manifest_path("ir_requirements_io.txt", path, sizeof(path)) == 0)
    {
        if (bs_adapter_plugin_validate_ir_requirements_ref(path) != 0)
            return -1;
    }

    return bs_adapter_io_register_providers(facade);
}
