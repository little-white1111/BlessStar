#include "bs/kernel/registry/registry_facade.h"

#include "bs/adapter/io/io_providers.h"
#include "bs/adapter/io/local_file_provider.h"
#include "bs/adapter/io/provider_stubs.h"

#include <cstring>

static int register_one(RegistryFacade* facade, const char* path, const char* hub_id,
                        IoProviderBinding* binding)
{
    PathEntry entry{};
    entry.source          = BS_PATH_ENTRY_PLUGIN;
    entry.manifest_ref    = "io-standard";
    entry.type_constraint = "io";

    if (bs_registry_facade_register_declaration(facade, path, &entry) != BS_REGISTRY_OK)
        return -1;
    if (bs_registry_facade_register_hub_mapping(facade, hub_id, path, 0) != BS_REGISTRY_OK)
        return -1;
    if (bs_registry_facade_bind_instance(facade, path, binding) != BS_REGISTRY_OK)
        return -1;
    return 0;
}

int bs_adapter_io_register_providers(RegistryFacade* facade)
{
    if (!facade)
        return -1;

    static LocalFileProvider* local = nullptr;
    if (!local)
        local = bs_adapter_io_local_provider_create();
    if (!local)
        return -1;

    if (register_one(facade, "/adapter/io/local", "adapter.io.local",
                     bs_adapter_io_local_provider_binding(local)) != 0)
        return -1;
    if (register_one(facade, "/adapter/io/db", "adapter.io.db", bs_adapter_io_db_stub_binding()) !=
        0)
        return -1;
    if (register_one(facade, "/adapter/io/remote", "adapter.io.remote",
                     bs_adapter_io_remote_stub_binding()) != 0)
        return -1;
    return 0;
}
