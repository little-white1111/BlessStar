#include "bs/kernel/io/io.h"
#include "bs/kernel/registry/registry_facade.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

static int mock_read(void* provider_ctx, const char* uri, IoReadResult* out, size_t max_read,
                     unsigned timeout_ms)
{
    (void)provider_ctx;
    (void)timeout_ms;
    (void)max_read;
    if (!out)
        return BS_IO_ERR_INVALID_ARG;
    bs_io_read_result_init(out);
    out->status = BS_IO_OK;
    const char payload[] = "ok";
    out->length = sizeof(payload) - 1;
    out->data   = static_cast<uint8_t*>(std::malloc(out->length));
    std::memcpy(out->data, payload, out->length);
    out->source_uri = static_cast<char*>(std::malloc(std::strlen(uri) + 1));
    std::strcpy(out->source_uri, uri);
    return BS_IO_OK;
}

static const IoProviderOps kMockOps = {
    BS_IO_PROVIDER_OPS_VERSION,
    mock_read,
    nullptr,
    nullptr,
};

int main()
{
    RegistryFacade* reg = bs_registry_facade_create();
    assert(reg != nullptr);
    assert(bs_registry_facade_advance_phase(reg, BS_REGISTRY_PHASE_P1) == BS_REGISTRY_OK);
    assert(bs_registry_facade_advance_phase(reg, BS_REGISTRY_PHASE_P2) == BS_REGISTRY_OK);

    PathEntry entry{};
    entry.source          = BS_PATH_ENTRY_PLUGIN;
    entry.manifest_ref    = "io";
    entry.type_constraint = "io";

    assert(bs_registry_facade_register_declaration(reg, "/adapter/io/local", &entry) ==
           BS_REGISTRY_OK);
    assert(bs_registry_facade_register_hub_mapping(reg, "adapter.io.local", "/adapter/io/local",
                                                   0) == BS_REGISTRY_OK);

    static IoProviderBinding binding = {&kMockOps, nullptr};
    assert(bs_registry_facade_bind_instance(reg, "/adapter/io/local", &binding) == BS_REGISTRY_OK);

    IoFacade* io = bs_io_facade_create(reg);
    assert(io != nullptr);

    IoReadResult result{};
    assert(bs_io_facade_read(io, "file://test.txt", &result) == BS_IO_OK);
    assert(result.length == 2);
    assert(result.data[0] == 'o');
    bs_io_read_result_free(&result);

    IoReadResult bad{};
    assert(bs_io_facade_read(io, "db://x", &bad) == BS_IO_ERR_UNSUPPORTED_SCHEME);
    bs_io_read_result_free(&bad);

    assert(bs_io_facade_read(io, "not-a-uri", &bad) == BS_IO_ERR_INVALID_URI);
    bs_io_read_result_free(&bad);

    bs_io_facade_destroy(io);
    bs_registry_facade_destroy(reg);
    return 0;
}
