#include "bs/kernel/io/io.h"
#include "bs/kernel/registry/registry_facade.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

static int mock_stat(void* provider_ctx, const char* uri, int64_t* out_size, int* out_exists)
{
    (void)provider_ctx;
    (void)uri;
    if (!out_size || !out_exists)
        return BS_IO_ERR_INVALID_ARG;
    *out_size   = 99;
    *out_exists = 1;
    return BS_IO_OK;
}

static const IoProviderOps kStatOnlyOps = {
    BS_IO_PROVIDER_OPS_VERSION,
    nullptr,
    mock_stat,
    nullptr,
};

int main()
{
    char path[BS_REGISTRY_MAX_PATH];

    assert(bs_io_provider_path_for_scheme("file", path, sizeof(path)) == BS_IO_OK);
    assert(std::strcmp(path, "/adapter/io/local") == 0);
    assert(bs_io_provider_path_for_scheme("db", path, sizeof(path)) == BS_IO_OK);
    assert(std::strcmp(path, "/adapter/io/db") == 0);
    assert(bs_io_provider_path_for_scheme("ftp", path, sizeof(path)) ==
           BS_IO_ERR_UNSUPPORTED_SCHEME);

    RegistryFacade* reg = bs_registry_facade_create();
    assert(reg != nullptr);
    IoFacade* io = bs_io_facade_create(reg);
    assert(io != nullptr);
    assert(bs_io_facade_registry(io) == reg);

    IoReadResult missing{};
    assert(bs_io_facade_read(io, "file://missing-provider", &missing) == BS_IO_ERR_REGISTRY);
    bs_io_read_result_free(&missing);

    assert(bs_registry_facade_advance_phase(reg, BS_REGISTRY_PHASE_P1) == BS_REGISTRY_OK);
    assert(bs_registry_facade_advance_phase(reg, BS_REGISTRY_PHASE_P2) == BS_REGISTRY_OK);

    PathEntry entry{};
    entry.source          = BS_PATH_ENTRY_PLUGIN;
    entry.manifest_ref    = "io";
    entry.type_constraint = "io";
    assert(bs_registry_facade_register_declaration(reg, "/adapter/io/local", &entry) ==
           BS_REGISTRY_OK);
    static IoProviderBinding binding = {&kStatOnlyOps, nullptr};
    assert(bs_registry_facade_bind_instance(reg, "/adapter/io/local", &binding) == BS_REGISTRY_OK);

    int64_t size   = 0;
    int     exists = 0;
    assert(bs_io_facade_stat(io, "file://x", &size, &exists) == BS_IO_OK);
    assert(size == 99 && exists == 1);

    assert(bs_io_facade_read(nullptr, "file://x", &missing) == BS_IO_ERR_INVALID_ARG);
    assert(bs_io_facade_read(io, nullptr, &missing) == BS_IO_ERR_INVALID_ARG);

    bs_io_facade_destroy(io);
    bs_registry_facade_destroy(reg);
    return 0;
}
