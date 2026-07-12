/*
 * PersistStoreStorage — StorageBackend 实现 (包装 BsAttachStore).
 *
 * ADR-workspace加固:
 *   不变量 #3 (路径安全): 拒绝 ".." 路径穿越。
 *   不变量 #5 (生产默认): 无 BS_TESTING 且无 BSTORAGE 覆盖时默认使用。
 *   不变量 #6 (不变量 #2 不削弱): 所有读写仍经过 bs_workspace_* C ABI。
 *
 * 注意: 此后端依赖 io_worker / BsAttachStore，不兼容沙箱环境。
 */

#include "bs/adapter/workspace/storage_persist.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

/* ─── Internal context ─────────────────────────────────────────────── */

struct PersistCtx {
    std::string    project_root;
    BsAttachStore* store;   /* 外部管理生命周期 */
};

/* ─── Helpers ──────────────────────────────────────────────────────── */

static PersistCtx* get_ctx(void* ctx)
{
    if (!ctx) return NULL;
    auto* sb = static_cast<StorageBackend*>(ctx);
    return static_cast<PersistCtx*>(sb->impl_ctx);
}

/* ─── Read via BsAttachStore ──────────────────────────────────────── */

static int persist_read(void* ctx, const char* path,
                         uint8_t** out_data, size_t* out_len)
{
    if (!ctx || !path || !out_data || !out_len) return -EINVAL;

    int safe = storage_path_safe(path);
    if (safe) return safe;

    auto* pctx = get_ctx(ctx);
    if (!pctx) return -EINVAL;

    std::string uri = std::string("workspace://") + path;

    uint64_t rev = 0;
    int rc = bs_adapter_attach_persist_store_get_revision(pctx->store,
                                                           uri.c_str(), &rev);
    if (rc != 0) return -ENOENT;

    /* ── FALLBACK to local direct read (store API is write-centric) ── */
    std::string full = pctx->project_root + "/" + path;
#ifdef _WIN32
    FILE* f = NULL;
    fopen_s(&f, full.c_str(), "rb");
#else
    FILE* f = fopen(full.c_str(), "rb");
#endif
    if (!f) return -ENOENT;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) { fclose(f); return -EIO; }
    rewind(f);

    uint8_t* buf = (uint8_t*)malloc((size_t)size + 1);
    if (!buf) { fclose(f); return -ENOMEM; }

    size_t n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[n] = '\0';
    *out_data = buf;
    *out_len  = n + 1;
    return 0;
}

/* ─── Write via BsAttachStore ─────────────────────────────────────── */

static int persist_write(void* ctx, const char* path,
                          const uint8_t* data, size_t len)
{
    if (!ctx || !path || !data) return -EINVAL;

    int safe = storage_path_safe(path);
    if (safe) return safe;

    auto* pctx = get_ctx(ctx);
    if (!pctx) return -EINVAL;

    std::string uri = std::string("workspace://") + path;

    size_t write_len = (len > 0) ? len - 1 : 0;
    int rc = bs_adapter_attach_persist_store_commit_per_path(
        pctx->store, uri.c_str(), data, write_len, 0);

    switch (rc) {
    case BS_ATTACH_OK:              return 0;
    case BS_ATTACH_ERR_INVALID_ARG: return -EINVAL;
    case BS_ATTACH_ERR_OOM:         return -ENOMEM;
    case BS_ATTACH_ERR_IO:          return -EIO;
    default:                        return -EIO;
    }
}

/* ─── Exists ───────────────────────────────────────────────────────── */

static int persist_exists(void* ctx, const char* path)
{
    if (!ctx || !path) return -EINVAL;

    int safe = storage_path_safe(path);
    if (safe) return safe;

    auto* pctx = get_ctx(ctx);
    if (!pctx) return -EINVAL;

    std::string uri = std::string("workspace://") + path;
    uint64_t rev = 0;
    int rc = bs_adapter_attach_persist_store_get_revision(pctx->store,
                                                           uri.c_str(), &rev);
    return (rc == 0) ? 1 : 0;
}

/* ─── Remove ───────────────────────────────────────────────────────── */

static int persist_remove(void* ctx, const char* path)
{
    if (!ctx || !path) return -EINVAL;

    int safe = storage_path_safe(path);
    if (safe) return safe;

    auto* pctx = get_ctx(ctx);
    if (!pctx) return -EINVAL;

    std::string full = pctx->project_root + "/" + path;
    if (remove(full.c_str()) != 0) {
        if (errno == ENOENT) return -ENOENT;
        return -EIO;
    }
    return 0;
}

/* ─── Destroy ──────────────────────────────────────────────────────── */

static void persist_destroy(void* ctx)
{
    if (!ctx) return;
    auto* sb = static_cast<StorageBackend*>(ctx);
    if (sb->impl_ctx) {
        auto* pctx = static_cast<PersistCtx*>(sb->impl_ctx);
        /* Close the BsAttachStore if created by create_storage */
        if (pctx->store) {
            bs_adapter_attach_persist_store_close(pctx->store);
            pctx->store = NULL;
        }
        delete pctx;
        sb->impl_ctx = NULL;
    }
}

/* ─── Constructor ──────────────────────────────────────────────────── */

StorageBackend* PersistStoreStorage_new(const char* project_root,
                                         BsAttachStore* store)
{
    if (!project_root || !store) return NULL;

    auto* ctx = new (std::nothrow) PersistCtx;
    if (!ctx) return NULL;
    ctx->project_root = project_root;
    ctx->store        = store;

    auto* sb = new (std::nothrow) StorageBackend;
    if (!sb) { delete ctx; return NULL; }

    sb->impl_ctx  = ctx;
    sb->read      = persist_read;
    sb->write     = persist_write;
    sb->exists    = persist_exists;
    sb->remove_fn = persist_remove;
    sb->destroy   = persist_destroy;

    return sb;
}
