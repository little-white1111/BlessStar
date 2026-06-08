#include "bs/adapter/attach_recover_sidecar.h"

#include "bs/adapter/attach_config.h"
#include "bs/adapter/attach_context.h"
#include "bs/adapter/attach_errors.h"
#include "bs/adapter/attach_session.h"
#include "bs/adapter/persistence/attach_runtime_sidecar.h"
#include "bs/adapter/persistence/attach_store.h"

#include <cstdlib>
#include <cstring>

#include <fstream>
#include <string>
#include <vector>

#if defined(BS_TESTING)
static int g_testing_sidecar_enabled = -1;
#endif

static int env_truthy(const char* value)
{
    if (!value || value[0] == '\0')
        return 0;
    return (std::strcmp(value, "1") == 0 || std::strcmp(value, "true") == 0 ||
            std::strcmp(value, "TRUE") == 0 || std::strcmp(value, "yes") == 0 ||
            std::strcmp(value, "YES") == 0)
               ? 1
               : 0;
}

int bs_adapter_attach_recover_sidecar_enabled(void)
{
#if defined(BS_TESTING)
    if (g_testing_sidecar_enabled >= 0)
        return g_testing_sidecar_enabled;
#endif
    const char* env = std::getenv("BS_ATTACH_RECOVER_SIDECAR");
    return env_truthy(env);
}

#if defined(BS_TESTING)
void bs_adapter_attach_recover_sidecar_testing_set_enabled(int enabled)
{
    g_testing_sidecar_enabled = enabled ? 1 : 0;
}
#endif

int bs_adapter_attach_recover_sidecar_invalidate(const char* manifest_path)
{
    return bs_adapter_attach_persist_sidecar_invalidate(manifest_path);
}

static int sidecar_may_fast_path(const char* manifest_path, BsAttachStore* store)
{
    if (!bs_adapter_attach_recover_sidecar_enabled())
        return 0;
    if (bs_adapter_attach_persist_store_had_exec_rollback(store, nullptr))
        return 0;
    return bs_adapter_attach_persist_sidecar_validate(
        manifest_path, store, BS_ATTACH_SIDECAR_FLAG_CLEAN_SHUTDOWN);
}

int bs_adapter_attach_recover_sidecar_write_ready(AttachContext* ctx, const char* manifest_path)
{
    if (!manifest_path)
        return BS_ATTACH_ERR_INVALID_ARG;
    (void)ctx;
    BsAttachStore* store = bs_adapter_attach_persist_store_open(manifest_path);
    if (!store)
        return BS_ATTACH_ERR_IO;
    const int rc = bs_adapter_attach_persist_sidecar_write(
        manifest_path, store, BS_ATTACH_SIDECAR_FLAG_CLEAN_SHUTDOWN);
    bs_adapter_attach_persist_store_close(store);
    return rc;
}

struct HydrateCtx
{
    AttachContext* ctx;
    BsAttachStore* store;
    int            rc;
};

static int hydrate_uri(const char* uri, uint64_t, void* user_ctx)
{
    auto* h = static_cast<HydrateCtx*>(user_ctx);
    if (!h || !h->ctx || !uri)
        return BS_ATTACH_ERR_INVALID_ARG;

    char canonical[4096];
    if (bs_adapter_attach_persist_store_get_canonical_path(h->store, uri, canonical,
                                                           sizeof(canonical)) != BS_ATTACH_OK)
    {
        h->rc = BS_ATTACH_ERR_IO;
        return BS_ATTACH_ERR_IO;
    }

    std::ifstream in(canonical, std::ios::binary);
    if (!in)
    {
        h->rc = BS_ATTACH_ERR_IO;
        return BS_ATTACH_ERR_IO;
    }
    in.seekg(0, std::ios::end);
    const std::streamoff sz = in.tellg();
    if (sz < 0)
    {
        h->rc = BS_ATTACH_ERR_IO;
        return BS_ATTACH_ERR_IO;
    }
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> payload(static_cast<size_t>(sz));
    if (sz > 0)
    {
        in.read(reinterpret_cast<char*>(payload.data()), sz);
        if (!in)
        {
            h->rc = BS_ATTACH_ERR_IO;
            return BS_ATTACH_ERR_IO;
        }
    }

    const int sync_rc = bs_adapter_attach_config_sync_path(
        h->ctx, uri, payload.empty() ? nullptr : payload.data(), payload.size());
    if (sync_rc != 0)
    {
        h->rc = sync_rc;
        return sync_rc;
    }
    const int post_rc = bs_adapter_attach_post_config_sync(h->ctx, uri, h->store);
    if (post_rc != 0)
    {
        h->rc = post_rc;
        return post_rc;
    }
    return BS_ATTACH_OK;
}

int bs_adapter_attach_recover_fast_hydrate(AttachContext* ctx, const char* manifest_path)
{
    if (!ctx || !manifest_path)
        return BS_ATTACH_ERR_INVALID_ARG;
    if (!bs_adapter_attach_ctx_is_log_bus_bound(ctx))
        return BS_ATTACH_ERR_INVALID_ARG;

    BsAttachStore* store = bs_adapter_attach_persist_store_open(manifest_path);
    if (!store)
        return BS_ATTACH_ERR_IO;

    if (!sidecar_may_fast_path(manifest_path, store))
    {
        bs_adapter_attach_persist_store_close(store);
        return BS_ATTACH_ERR_IO;
    }

    bs_adapter_attach_session_begin_write_window(ctx);
    HydrateCtx h{ctx, store, BS_ATTACH_OK};
    const int  foreach_rc =
        bs_adapter_attach_persist_store_foreach_uri(store, hydrate_uri, &h);
    bs_adapter_attach_persist_store_close(store);
    bs_adapter_attach_session_end_write_window(ctx);

    if (foreach_rc != BS_ATTACH_OK)
        return foreach_rc;
    return h.rc;
}

int bs_adapter_attach_recover_sidecar_can_fast_hydrate(const char* manifest_path)
{
    if (!manifest_path)
        return 0;
    BsAttachStore* store = bs_adapter_attach_persist_store_open(manifest_path);
    if (!store)
        return 0;
    const int ok = sidecar_may_fast_path(manifest_path, store);
    bs_adapter_attach_persist_store_close(store);
    return ok;
}
