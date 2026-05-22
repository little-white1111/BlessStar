#include "bs/adapter/io/io_providers.h"
#include "bs/adapter/io/provider_stubs.h"
#include "bs/kernel/io/io.h"
#include "bs/kernel/registry/registry_facade.h"

#include <cassert>
#include <cstring>

static void expect_stub_read_fails(IoProviderBinding* binding, const char* uri)
{
    IoReadResult r{};
    assert(binding && binding->ops && binding->ops->read);
    assert(binding->ops->read(binding->ctx, uri, &r, 1024, 30000) == BS_IO_ERR_PROVIDER);
    assert(r.error_message != nullptr);
    bs_io_read_result_free(&r);
}

int main()
{
    RegistryFacade* reg = bs_registry_facade_create();
    assert(reg != nullptr);
    assert(bs_registry_facade_advance_phase(reg, BS_REGISTRY_PHASE_P1) == BS_REGISTRY_OK);
    assert(bs_registry_facade_advance_phase(reg, BS_REGISTRY_PHASE_P2) == BS_REGISTRY_OK);
    assert(bs_adapter_io_register_providers(reg) == 0);

    Binding db{};
    assert(bs_registry_facade_resolve(reg, "/adapter/io/db", &db) == BS_REGISTRY_OK);
    expect_stub_read_fails(static_cast<IoProviderBinding*>(db.impl), "db://cfg/t");

    Binding remote{};
    assert(bs_registry_facade_resolve(reg, "/adapter/io/remote", &remote) == BS_REGISTRY_OK);
    expect_stub_read_fails(static_cast<IoProviderBinding*>(remote.impl), "remote://host/cfg");

    Binding local{};
    assert(bs_registry_facade_resolve(reg, "adapter.io.local", &local) == BS_REGISTRY_OK);
    assert(local.impl != nullptr);

    IoFacade* io = bs_io_facade_create(reg);
    IoReadResult facade_db{};
    assert(bs_io_facade_read(io, "db://x", &facade_db) == BS_IO_ERR_UNSUPPORTED_SCHEME);
    bs_io_read_result_free(&facade_db);

    bs_io_facade_destroy(io);
    bs_registry_facade_destroy(reg);
    return 0;
}
