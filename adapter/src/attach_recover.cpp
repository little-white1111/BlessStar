#include "bs/adapter/attach_config.h"
#include "bs/adapter/attach_errors.h"
#include "bs/adapter/attach_ir_snapshot.h"
#include "bs/adapter/attach_recover.h"
#include "bs/adapter/attach_recover_sidecar.h"
#include "bs/adapter/attach_session.h"
#include "bs/adapter/orchestration/reload_with_report.h"
#include "bs/adapter/persistence/attach_store.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
std::mutex                                      g_recover_manifest_mu;
std::unordered_map<AttachContext*, std::string> g_recover_manifest;

struct RecoverUriList
{
    std::vector<std::string> uris;
};

int collect_uri(const char* uri, uint64_t, void* user_ctx)
{
    auto* list = static_cast<RecoverUriList*>(user_ctx);
    if (!uri || !list)
        return BS_ATTACH_ERR_INVALID_ARG;
    list->uris.emplace_back(uri);
    return BS_ATTACH_OK;
}

int facade_read_fn(void* user_ctx, const char* uri, IoReadResult* out)
{
    return bs_io_facade_read(static_cast<IoFacade*>(user_ctx), uri, out);
}

std::string manifest_for(AttachContext* ctx, const BsAttachRecoverColdReloadOptions* opts)
{
    if (opts && opts->manifest_path && opts->manifest_path[0] != '\0')
        return opts->manifest_path;
    std::lock_guard<std::mutex> lock(g_recover_manifest_mu);
    const auto                  it = g_recover_manifest.find(ctx);
    return it == g_recover_manifest.end() ? std::string() : it->second;
}

void remember_manifest(AttachContext* ctx, const char* manifest_path)
{
    if (!ctx || !manifest_path)
        return;
    std::lock_guard<std::mutex> lock(g_recover_manifest_mu);
    g_recover_manifest[ctx] = manifest_path;
}

void forget_manifest(AttachContext* ctx)
{
    std::lock_guard<std::mutex> lock(g_recover_manifest_mu);
    g_recover_manifest.erase(ctx);
}

struct ReconcileCtx
{
    AttachContext* ctx;
    BsAttachStore* store;
    int            rc;
};

int reconcile_uri(const char* uri, uint64_t rev, void* user_ctx)
{
    auto* r = static_cast<ReconcileCtx*>(user_ctx);
    if (!r || !r->ctx || !uri)
        return BS_ATTACH_ERR_INVALID_ARG;
    if (!bs_adapter_attach_config_has_manager(r->ctx))
        return BS_ATTACH_OK;

    uint64_t manifest_rev = rev;
    if (r->store &&
        bs_adapter_attach_persist_store_get_revision(r->store, uri, &manifest_rev) == BS_ATTACH_OK)
    {
        rev = manifest_rev;
    }

    BsAttachSnapshotMeta meta{};
    const int            meta_rc = bs_adapter_attach_config_get_snapshot_meta(r->ctx, uri, &meta);
    if (meta_rc == BS_ATTACH_ERR_RECOVERING)
        return BS_ATTACH_OK;
    if (meta_rc != 0)
    {
        r->rc = meta_rc;
        return meta_rc;
    }
    if (meta.revision != rev)
    {
        r->rc = BS_ATTACH_ERR_CONFLICT;
        return BS_ATTACH_ERR_CONFLICT;
    }
    return BS_ATTACH_OK;
}

int reconcile_cm_with_manifest(AttachContext* ctx, const char* manifest_path)
{
    if (!ctx || !manifest_path)
        return BS_ATTACH_ERR_INVALID_ARG;
    BsAttachStore* store = bs_adapter_attach_persist_store_open(manifest_path);
    if (!store)
        return BS_ATTACH_ERR_IO;
    ReconcileCtx r{ctx, store, BS_ATTACH_OK};
    const int    rc = bs_adapter_attach_persist_store_foreach_uri(store, reconcile_uri, &r);
    bs_adapter_attach_persist_store_close(store);
    if (rc != BS_ATTACH_OK)
        return rc;
    return r.rc;
}
} // namespace

AttachContext* bs_adapter_attach_recover_from_store(const char* manifest_path,
                                                    const BsAttachRecoverFromStoreOptions* opts)
{
    (void)opts;
    if (!manifest_path || manifest_path[0] == '\0')
        return nullptr;

    BsAttachStore* store = bs_adapter_attach_persist_store_open(manifest_path);
    if (!store)
        return nullptr;
    bs_adapter_attach_persist_store_close(store);

    AttachContext* ctx = bs_adapter_attach_ctx_create();
    if (!ctx)
        return nullptr;
    bs_adapter_attach_session_set_recovering(ctx, 1);
    remember_manifest(ctx, manifest_path);
    return ctx;
}

int bs_adapter_attach_recover_cold_reload(AttachContext*                          ctx,
                                          const BsAttachRecoverColdReloadOptions* opts)
{
    if (!ctx)
        return BS_ATTACH_ERR_INVALID_ARG;
    const std::string manifest_path = manifest_for(ctx, opts);
    if (manifest_path.empty())
        return BS_ATTACH_ERR_INVALID_ARG;

    BsAttachStore* store = bs_adapter_attach_persist_store_open(manifest_path.c_str());
    if (!store)
        return BS_ATTACH_ERR_IO;

    RecoverUriList uris;
    int            rc = bs_adapter_attach_persist_store_foreach_uri(store, collect_uri, &uris);
    bs_adapter_attach_persist_store_close(store);
    if (rc != BS_ATTACH_OK)
        return rc;

    if (bs_adapter_attach_ctx_rebuild_kernel_pool(ctx) != 0)
        return BS_ATTACH_ERR_IO;

    if (uris.uris.empty())
        return BS_ATTACH_ERR_IO;

    if (bs_adapter_attach_recover_sidecar_can_fast_hydrate(manifest_path.c_str()))
    {
        const int hydrate_rc = bs_adapter_attach_recover_fast_hydrate(ctx, manifest_path.c_str());
        if (hydrate_rc == BS_ATTACH_OK)
        {
            const int reconcile_rc = reconcile_cm_with_manifest(ctx, manifest_path.c_str());
            if (reconcile_rc != BS_ATTACH_OK)
                return reconcile_rc;
            bs_adapter_attach_session_set_recovering(ctx, 0);
            forget_manifest(ctx);
            (void)bs_adapter_attach_recover_sidecar_write_ready(ctx, manifest_path.c_str());
            return BS_ATTACH_OK;
        }
    }

    ReloadPathReadFn read_fn  = opts ? opts->read_fn : nullptr;
    void*            read_ctx = opts ? opts->read_ctx : nullptr;
    if (!read_fn && opts && opts->io_facade)
    {
        read_fn  = facade_read_fn;
        read_ctx = opts->io_facade;
    }
    if (!read_fn)
        return BS_ATTACH_ERR_INVALID_ARG;

    ReloadBatchController* ctrl = bs_adapter_attach_reload_batch_create(
        opts && opts->max_inflight ? opts->max_inflight : (unsigned)uris.uris.size());
    if (!ctrl)
        return BS_ATTACH_ERR_OOM;

    bs_adapter_attach_reload_batch_set_attach_ctx(ctrl, ctx);
    bs_adapter_attach_reload_batch_set_manifest_path(ctrl, manifest_path.c_str());
    bs_adapter_attach_reload_batch_set_attach_scheme(
        ctrl, (opts && opts->scheme != BS_ATTACH_SCHEME_UNSET) ? opts->scheme
                                                               : BS_ATTACH_SCHEME_PER_PATH);
    bs_adapter_attach_reload_batch_set_read_fn(ctrl, read_fn, read_ctx);
    if (opts && opts->gate_fn)
        bs_adapter_attach_reload_batch_set_gate_fn(ctrl, opts->gate_fn, opts->gate_ctx);
    else
        bs_adapter_attach_reload_batch_set_default_gate(ctrl);
    if (opts && opts->session_memory_cap != 0)
        bs_adapter_attach_reload_batch_set_session_memory_cap(ctrl, opts->session_memory_cap);

    for (const std::string& uri : uris.uris)
    {
        if (bs_adapter_attach_reload_batch_add_path(ctrl, uri.c_str()) != 0)
        {
            bs_adapter_attach_reload_batch_destroy(ctrl);
            return BS_ATTACH_ERR_LIMIT;
        }
    }

    rc = bs_adapter_attach_reload_batch_run_with_report(ctrl, opts ? opts->report : nullptr);
    const BatchOutcome outcome = bs_adapter_attach_reload_batch_outcome(ctrl);
    bs_adapter_attach_reload_batch_destroy(ctrl);

    if (rc == 0 && outcome == BATCH_ALL_OK)
    {
        const int reconcile_rc = reconcile_cm_with_manifest(ctx, manifest_path.c_str());
        if (reconcile_rc != BS_ATTACH_OK)
            return reconcile_rc;
        bs_adapter_attach_session_set_recovering(ctx, 0);
        forget_manifest(ctx);
        if (bs_adapter_attach_recover_sidecar_enabled())
            (void)bs_adapter_attach_recover_sidecar_write_ready(ctx, manifest_path.c_str());
        return BS_ATTACH_OK;
    }
    return (rc != 0) ? rc : BS_ATTACH_ERR_IO;
}
