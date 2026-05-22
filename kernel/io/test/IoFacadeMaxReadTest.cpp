#include "bs/kernel/io/io.h"
#include "bs/kernel/registry/registry_facade.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

static size_t g_last_max_read = 0;

static int mock_read_cap(void* provider_ctx, const char* uri, IoReadResult* out, size_t max_read,
                         unsigned timeout_ms)
{
    (void)provider_ctx;
    (void)uri;
    (void)timeout_ms;
    if (!out)
        return BS_IO_ERR_INVALID_ARG;
    bs_io_read_result_init(out);

    g_last_max_read = max_read;

    const size_t payload_len = 64;
    const size_t cap         = max_read < payload_len ? max_read : payload_len;
    if (cap > 0)
    {
        out->data = static_cast<uint8_t*>(std::malloc(cap));
        for (size_t i = 0; i < cap; ++i)
            out->data[i] = static_cast<uint8_t>(i);
    }
    out->length    = cap;
    out->truncated = (max_read < payload_len) ? 1 : 0;
    out->status    = BS_IO_OK;
    return BS_IO_OK;
}

static const IoProviderOps kCapOps = {
    BS_IO_PROVIDER_OPS_VERSION,
    mock_read_cap,
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
    assert(bs_registry_facade_register_declaration(reg, "/adapter/io/local", &entry) == BS_REGISTRY_OK);
    assert(bs_registry_facade_register_hub_mapping(reg, "adapter.io.local", "/adapter/io/local", 0) ==
           BS_REGISTRY_OK);

    static IoProviderBinding binding = {&kCapOps, nullptr};
    assert(bs_registry_facade_bind_instance(reg, "/adapter/io/local", &binding) == BS_REGISTRY_OK);

    IoFacade* io = bs_io_facade_create(reg);
    IoReadResult result{};
    assert(bs_io_facade_read(io, "file://cap", &result) == BS_IO_OK);
    assert(g_last_max_read == BS_IO_MAX_READ_BYTES);
    assert(result.length == 64);
    assert(result.truncated == 0);
    bs_io_read_result_free(&result);

    IoReadResult small{};
    assert(binding.ops->read(binding.ctx, "file://x", &small, 8, 30000) == BS_IO_OK);
    assert(small.length == 8);
    assert(small.truncated == 1);
    bs_io_read_result_free(&small);

    bs_io_facade_destroy(io);
    bs_registry_facade_destroy(reg);
    return 0;
}
