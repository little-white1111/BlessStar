#include "bs/adapter/attach_config.h"
#include "bs/adapter/attach_errors.h"
#include "bs/adapter/attach_session.h"

#include "bs/adapter/parser/json_lexer.h"

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "attach_context_internal.h"

namespace
{
struct SnapshotReadHandle
{
    int                     used = 0;
    std::string             path;
    uint64_t                revision = 0;
    std::shared_ptr<std::vector<unsigned char>> blob;
};

struct AttachSessionState
{
    std::shared_mutex              session_mu;
    std::mutex                     wait_mu;
    std::condition_variable        wait_cv;
    std::atomic<int>               active_readers{0};
    std::atomic<int>               write_depth{0};
    std::atomic<bool>              block_new_reads{false};
    std::atomic<bool>              closing{false};
    std::unordered_map<std::string, uint64_t> path_revision;
    std::mutex                     rev_mu;
    SnapshotReadHandle             handles[16];
};

thread_local int g_attach_read_lock_depth = 0;

AttachSessionState* session_of(AttachContext* ctx)
{
    return ctx ? static_cast<AttachSessionState*>(ctx->session_state) : nullptr;
}

uint32_t default_chunk_cap(void)
{
    const uint64_t cap = BS_JSON_MAX_INPUT_BYTES;
    return cap > 0xFFFFFFFFu ? 0xFFFFFFFFu : static_cast<uint32_t>(cap);
}

} // namespace

void bs_adapter_attach_session_init(AttachContext* ctx)
{
    if (!ctx || ctx->session_state)
        return;
    ctx->session_state = new AttachSessionState();
}

void bs_adapter_attach_session_destroy(AttachContext* ctx)
{
    if (!ctx || !ctx->session_state)
        return;
    auto* st = session_of(ctx);
    st->closing.store(true);
    st->block_new_reads.store(true);
    {
        std::unique_lock<std::shared_mutex> lock(st->session_mu);
    }
    delete st;
    ctx->session_state = nullptr;
}

void bs_adapter_attach_session_begin_write_window(AttachContext* ctx)
{
    auto* st = session_of(ctx);
    if (!st)
        return;
    st->block_new_reads.store(true);
    std::unique_lock<std::mutex> w(st->wait_mu);
    st->wait_cv.wait(w, [&] { return st->active_readers.load() == 0; });
    st->session_mu.lock();
    st->write_depth.fetch_add(1);
}

void bs_adapter_attach_session_end_write_window(AttachContext* ctx)
{
    auto* st = session_of(ctx);
    if (!st || st->write_depth.load() == 0)
        return;
    st->session_mu.unlock();
    if (st->write_depth.fetch_sub(1) == 1)
        st->block_new_reads.store(false);
    st->wait_cv.notify_all();
}

int bs_adapter_attach_session_try_read_lock(AttachContext* ctx)
{
    auto* st = session_of(ctx);
    if (!st)
        return 0;
    if (st->closing.load())
        return BS_ATTACH_CONC_ERR_CLOSED;
    if (st->block_new_reads.load() && g_attach_read_lock_depth == 0)
        return BS_ATTACH_CONC_ERR_READ_BLOCKED;
    st->session_mu.lock_shared();
    ++g_attach_read_lock_depth;
    st->active_readers.fetch_add(1);
    return 0;
}

void bs_adapter_attach_session_read_unlock(AttachContext* ctx)
{
    auto* st = session_of(ctx);
    if (!st || g_attach_read_lock_depth == 0)
        return;
    --g_attach_read_lock_depth;
    st->session_mu.unlock_shared();
    if (st->active_readers.fetch_sub(1) == 1)
    {
        std::lock_guard<std::mutex> w(st->wait_mu);
        st->wait_cv.notify_all();
    }
}

int bs_adapter_attach_session_try_write_lock(AttachContext* ctx)
{
    auto* st = session_of(ctx);
    if (!st)
        return 0;
    if (st->closing.load())
        return BS_ATTACH_CONC_ERR_CLOSED;
    st->session_mu.lock();
    st->write_depth.fetch_add(1);
    return 0;
}

void bs_adapter_attach_session_write_unlock(AttachContext* ctx)
{
    auto* st = session_of(ctx);
    if (!st || st->write_depth.load() == 0)
        return;
    st->session_mu.unlock();
    st->write_depth.fetch_sub(1);
}

uint64_t bs_adapter_attach_session_path_revision(AttachContext* ctx, const char* path)
{
    auto* st = session_of(ctx);
    if (!st || !path)
        return 0;
    std::lock_guard<std::mutex> lock(st->rev_mu);
    const auto it = st->path_revision.find(path);
    return it == st->path_revision.end() ? 0 : it->second;
}

void bs_adapter_attach_session_bump_revision(AttachContext* ctx, const char* path)
{
    auto* st = session_of(ctx);
    if (!st || !path)
        return;
    std::lock_guard<std::mutex> lock(st->rev_mu);
    st->path_revision[path] = st->path_revision[path] + 1;
}

int bs_adapter_attach_session_in_write_window(AttachContext* ctx)
{
    auto* st = session_of(ctx);
    return (st && st->write_depth.load() > 0) ? 1 : 0;
}

static int snapshot_bytes(AttachContext* ctx, const char* config_path, size_t* total,
                          std::shared_ptr<std::vector<unsigned char>>* blob_out)
{
    void*  snap = nullptr;
    size_t sz   = 0;
    const int rc = bs_adapter_attach_config_get_snapshot(ctx, config_path, &snap, &sz);
    if (rc != 0)
        return rc;
    auto blob = std::make_shared<std::vector<unsigned char>>(sz);
    if (sz > 0 && snap)
        std::memcpy(blob->data(), snap, sz);
    std::free(snap);
    *total   = sz;
    *blob_out = blob;
    return 0;
}

int bs_adapter_attach_config_get_snapshot_meta(AttachContext* ctx, const char* config_path,
                                                BsAttachSnapshotMeta* out)
{
    if (!out)
        return -1;
    out->revision   = 0;
    out->total_size = 0;
    out->chunk_cap  = default_chunk_cap();

    const int lk = bs_adapter_attach_session_try_read_lock(ctx);
    if (lk != 0)
        return lk;

    size_t total = 0;
    std::shared_ptr<std::vector<unsigned char>> blob;
    const int rc = snapshot_bytes(ctx, config_path, &total, &blob);
    if (rc == 0)
    {
        out->total_size = total;
        out->revision   = bs_adapter_attach_session_path_revision(ctx, config_path);
        if (out->revision == 0 && total > 0)
            out->revision = 1;
    }
    bs_adapter_attach_session_read_unlock(ctx);
    return rc;
}

int bs_adapter_attach_config_get_snapshot_copy(AttachContext* ctx, const char* config_path,
                                               void* buf, size_t buf_cap, size_t* out_size,
                                               uint64_t* revision_out)
{
    if (!buf || !out_size || !revision_out)
        return -1;

    const int lk = bs_adapter_attach_session_try_read_lock(ctx);
    if (lk != 0)
        return lk;

    size_t total = 0;
    std::shared_ptr<std::vector<unsigned char>> blob;
    int    rc = snapshot_bytes(ctx, config_path, &total, &blob);
    if (rc == 0)
    {
        *revision_out = bs_adapter_attach_session_path_revision(ctx, config_path);
        if (*revision_out == 0 && total > 0)
            *revision_out = 1;
        if (total > BS_JSON_MAX_INPUT_BYTES)
            rc = BS_ATTACH_CONC_ERR_TOO_LARGE;
        else if (total > buf_cap)
            rc = -1;
        else
        {
            if (total > 0)
                std::memcpy(buf, blob->data(), total);
            *out_size = total;
        }
    }
    bs_adapter_attach_session_read_unlock(ctx);
    return rc;
}

int bs_adapter_attach_config_open_snapshot_read(AttachContext* ctx, const char* config_path,
                                                int* handle_out, uint64_t* revision_out)
{
    if (!handle_out || !revision_out)
        return -1;

    const int lk = bs_adapter_attach_session_try_read_lock(ctx);
    if (lk != 0)
        return lk;

    size_t total = 0;
    std::shared_ptr<std::vector<unsigned char>> blob;
    int    rc = snapshot_bytes(ctx, config_path, &total, &blob);
    if (rc != 0)
    {
        bs_adapter_attach_session_read_unlock(ctx);
        return rc;
    }

    uint64_t rev = bs_adapter_attach_session_path_revision(ctx, config_path);
    if (rev == 0 && total > 0)
        rev = 1;

    bs_adapter_attach_session_read_unlock(ctx);

    auto* st = session_of(ctx);
    if (!st)
        return -1;

    for (auto& h : st->handles)
    {
        if (!h.used)
        {
            h.used     = 1;
            h.path     = config_path ? config_path : "";
            h.revision = rev;
            h.blob     = blob;
            *handle_out    = static_cast<int>(&h - st->handles);
            *revision_out  = rev;
            return 0;
        }
    }
    return BS_ATTACH_CONC_ERR_INVALID_HANDLE;
}

int bs_adapter_attach_config_read_snapshot_chunk(AttachContext* ctx, int handle, size_t offset,
                                                 void* buf, size_t buf_cap, size_t* out_len)
{
    if (!buf || !out_len || buf_cap == 0)
        return -1;
    *out_len = 0;

    const int lk = bs_adapter_attach_session_try_read_lock(ctx);
    if (lk != 0)
        return lk;

    auto* st = session_of(ctx);
    if (!st || handle < 0 || handle >= 16 || !st->handles[handle].used)
    {
        bs_adapter_attach_session_read_unlock(ctx);
        return BS_ATTACH_CONC_ERR_INVALID_HANDLE;
    }

    const SnapshotReadHandle& h = st->handles[handle];
    const uint64_t          cur = bs_adapter_attach_session_path_revision(ctx, h.path.c_str());
    if (cur != h.revision)
    {
        bs_adapter_attach_session_read_unlock(ctx);
        return BS_ATTACH_CONC_ERR_REVISION_CHANGED;
    }

    const size_t total = h.blob ? h.blob->size() : 0;
    if (offset >= total)
    {
        bs_adapter_attach_session_read_unlock(ctx);
        return 0;
    }

    const size_t chunk_cap =
        static_cast<size_t>(default_chunk_cap() < buf_cap ? default_chunk_cap() : buf_cap);
    const size_t n = total - offset;
    const size_t copy_n = n < chunk_cap ? n : chunk_cap;
    if (copy_n > 0 && h.blob)
        std::memcpy(buf, h.blob->data() + offset, copy_n);
    *out_len = copy_n;
    bs_adapter_attach_session_read_unlock(ctx);
    return 0;
}

void bs_adapter_attach_config_close_snapshot_read(AttachContext* ctx, int handle)
{
    auto* st = session_of(ctx);
    if (!st || handle < 0 || handle >= 16)
        return;
    st->handles[handle].used = 0;
    st->handles[handle].blob.reset();
    st->handles[handle].path.clear();
}
