#include "bs/adapter/io/provider_stubs.h"

#include <cstdlib>
#include <cstring>

struct DbProviderStub
{
    IoProviderBinding binding{};
};

static char* dup_cstr(const char* s)
{
    if (!s)
        return nullptr;
    const size_t n = std::strlen(s);
    auto*        p = static_cast<char*>(std::malloc(n + 1));
    if (!p)
        return nullptr;
    std::memcpy(p, s, n + 1);
    return p;
}

static int stub_read(void* provider_ctx, const char* uri, IoReadResult* out, size_t max_read,
                     unsigned timeout_ms)
{
    (void)provider_ctx;
    (void)uri;
    (void)max_read;
    (void)timeout_ms;
    if (!out)
        return BS_IO_ERR_INVALID_ARG;
    bs_io_read_result_init(out);
    out->status        = BS_IO_ERR_PROVIDER;
    out->error_message = dup_cstr("db provider stub: not implemented");
    return BS_IO_ERR_PROVIDER;
}

static int stub_stat(void* provider_ctx, const char* uri, int64_t* out_size, int* out_exists)
{
    (void)provider_ctx;
    (void)uri;
    (void)out_size;
    (void)out_exists;
    return BS_IO_ERR_PROVIDER;
}

static void stub_destroy(void* provider_ctx)
{
    delete static_cast<DbProviderStub*>(provider_ctx);
}

static const IoProviderOps kDbOps = {
    BS_IO_PROVIDER_OPS_VERSION,
    stub_read,
    stub_stat,
    stub_destroy,
};

static DbProviderStub* db_instance(void)
{
    static DbProviderStub* inst = []() {
        auto* p        = new DbProviderStub();
        p->binding.ops = &kDbOps;
        p->binding.ctx = p;
        return p;
    }();
    return inst;
}

IoProviderBinding* bs_adapter_io_db_stub_binding(void)
{
    return &db_instance()->binding;
}
