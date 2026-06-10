#include "bs/kernel/common/bs_reentrancy.h"
#include "bs/kernel/common/bs_wait_trace.h"

#include "bs/adapter/attach_config.h"
#include "bs/adapter/attach_errors.h"
#include "bs/adapter/attach_session.h"
#include "bs/adapter/parser/json_lexer.h"
#include "bs/adapter/persistence/attach_store.h"

#include "bs/kernel/state/StateSnapshotRcu.h"

#include <chrono>
#include <condition_variable>
#include <cstring>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(BS_TESTING)
#include <assert.h>
#endif

#include "attach_context_internal.h"
#include "attach_notify_queue_internal.h"

namespace
{
struct SnapshotReadHandle
{
    int                                           used = 0;
    std::string                                   path;
    uint64_t                                      revision = 0;
    std::shared_ptr<const BsStateSnapshotPayload> payload;
};

struct AttachSessionState
{
    std::mutex                                wait_mu;
    std::condition_variable                   wait_cv;
    std::atomic<uint64_t>                     read_epoch{0};
    std::atomic<int>                          active_readers{0};
    std::atomic<int>                          write_depth{0};
    std::atomic<bool>                         block_new_reads{false};
    std::atomic<bool>                         closing{false};
    std::atomic<bool>                         recovering{false};
    std::unordered_map<std::string, uint64_t> path_revision;
    std::mutex                                rev_mu;
    std::condition_variable                   rev_cv;
    SnapshotReadHandle                        handles[16];
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

static void session_wait_active_readers_zero_traced(AttachSessionState*           st,
                                                    std::unique_lock<std::mutex>& w,
                                                    const char*                   site)
{
    const int hang_t0 = bs_wait_trace_hang_begin(site);
    while (st->active_readers.load() != 0)
    {
        bs_wait_trace_hang_tick_u64(site, hang_t0,
                                    static_cast<unsigned long long>(st->active_readers.load()));
        st->wait_cv.wait_for(w, std::chrono::milliseconds(500),
                             [&] { return st->active_readers.load() == 0; });
    }
    if (hang_t0 >= 0)
        bs_wait_trace_hang_end(site, hang_t0);
}

static void cancel_all_snapshot_handles(AttachSessionState* st)
{
    if (!st)
        return;
    for (auto& h : st->handles)
    {
        h.used     = 0;
        h.revision = 0;
        h.path.clear();
        h.payload.reset();
    }
}

} // namespace

void bs_adapter_attach_session_init(AttachContext* ctx)
{
    if (!ctx || ctx->session_state)
        return;
    ctx->session_state = new AttachSessionState();
    ctx->notify_queue  = nullptr;
    bs_adapter_attach_notify_queue_bind(ctx);
}

void bs_adapter_attach_session_destroy(AttachContext* ctx)
{
    if (!ctx || !ctx->session_state)
        return;
    auto* st = session_of(ctx);
    st->closing.store(true);
    st->block_new_reads.store(true);
    cancel_all_snapshot_handles(st);
    bs_adapter_attach_config_clear_phase2_notify(ctx);
    bs_adapter_attach_notify_queue_shutdown(ctx);
    {
        std::unique_lock<std::mutex> w(st->wait_mu);
        session_wait_active_readers_zero_traced(st, w,
                                                "attach_session:destroy_wait_active_readers");
    }
#if defined(BS_TESTING)
    assert(st->active_readers.load() == 0);
    assert(g_attach_read_lock_depth == 0);
#endif
    delete st;
    ctx->session_state = nullptr;
}

void bs_adapter_attach_session_begin_write_window(AttachContext* ctx)
{
    auto* st = session_of(ctx);
    if (!st)
        return;
    const bool nested = st->write_depth.load() > 0;
    if (!nested)
    {
        /* Same-thread read guard imbalance must not block write-window open (smoke_fail_ci). */
        while (g_attach_read_lock_depth > 0)
            bs_adapter_attach_session_read_unlock(ctx);
        st->block_new_reads.store(true);
        st->read_epoch.fetch_add(1);
    }
    st->write_depth.fetch_add(1);
    bs_reentrancy_enter_attach_write();
    bs_reentrancy_enter_attach_write_window();
}

void bs_adapter_attach_session_end_write_window(AttachContext* ctx)
{
    auto* st = session_of(ctx);
    if (!st || st->write_depth.load() == 0)
        return;

    const int closing_outer_window = (st->write_depth.load() == 1);

    bs_reentrancy_leave_attach_write();
    bs_reentrancy_leave_attach_write_window();
    if (closing_outer_window)
        st->block_new_reads.store(false);
    st->write_depth.fetch_sub(1);
    st->wait_cv.notify_all();

    if (closing_outer_window)
    {
        bs_adapter_attach_config_drain_deferred_events(ctx);
        bs_adapter_attach_notify_queue_flush(ctx);
    }
}

void bs_adapter_attach_session_drain_pending_notifications(AttachContext* ctx)
{
    bs_adapter_attach_notify_queue_flush(ctx);
}

int bs_adapter_attach_session_try_read_lock(AttachContext* ctx)
{
    auto* st = session_of(ctx);
    if (!st)
        return 0;
    if (st->closing.load())
        return BS_ATTACH_CONC_ERR_CLOSED;
    if (st->recovering.load() && g_attach_read_lock_depth == 0)
        return BS_ATTACH_ERR_RECOVERING;
    if (st->block_new_reads.load() && g_attach_read_lock_depth == 0)
        return BS_ATTACH_CONC_ERR_READ_BLOCKED;
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
    if (st->write_depth.load() > 0)
    {
        st->write_depth.fetch_add(1);
        bs_reentrancy_enter_attach_write();
        return 0;
    }
    st->block_new_reads.store(true);
    int expected = 0;
    if (!st->write_depth.compare_exchange_strong(expected, 1))
    {
        st->block_new_reads.store(false);
        return BS_ATTACH_CONC_ERR_READ_BLOCKED;
    }
    bs_reentrancy_enter_attach_write();
    return 0;
}

void bs_adapter_attach_session_write_unlock(AttachContext* ctx)
{
    auto* st = session_of(ctx);
    if (!st || st->write_depth.load() == 0)
        return;
    bs_reentrancy_leave_attach_write();
    if (st->write_depth.fetch_sub(1) == 1)
        st->block_new_reads.store(false);
}

uint64_t bs_adapter_attach_session_path_revision(AttachContext* ctx, const char* path)
{
    auto* st = session_of(ctx);
    if (!st || !path)
        return 0;
    std::lock_guard<std::mutex> lock(st->rev_mu);
    const auto                  it = st->path_revision.find(path);
    return it == st->path_revision.end() ? 0 : it->second;
}

void bs_adapter_attach_session_bump_revision(AttachContext* ctx, const char* path)
{
    auto* st = session_of(ctx);
    if (!st || !path)
        return;
    std::lock_guard<std::mutex> lock(st->rev_mu);
    st->path_revision[path] = st->path_revision[path] + 1;
    st->rev_cv.notify_all();
}

void bs_adapter_attach_session_set_path_revision(AttachContext* ctx, const char* path,
                                                 uint64_t revision)
{
    auto* st = session_of(ctx);
    if (!st || !path)
        return;
    std::lock_guard<std::mutex> lock(st->rev_mu);
    st->path_revision[path] = revision;
    st->rev_cv.notify_all();
}

int bs_adapter_attach_session_in_write_window(AttachContext* ctx)
{
    auto* st = session_of(ctx);
    return (st && st->write_depth.load() > 0) ? 1 : 0;
}

void bs_adapter_attach_session_set_recovering(AttachContext* ctx, int recovering)
{
    auto* st = session_of(ctx);
    if (!st)
        return;
    st->recovering.store(recovering ? true : false);
}

int bs_adapter_attach_session_is_recovering(AttachContext* ctx)
{
    auto* st = session_of(ctx);
    return (st && st->recovering.load()) ? 1 : 0;
}

static int snapshot_pin(AttachContext* ctx, const char* config_path,
                        std::shared_ptr<const BsStateSnapshotPayload>* payload_out)
{
    if (!payload_out)
        return -1;
    payload_out->reset();
    return bs_adapter_attach_config_snapshot_pin(ctx, config_path, payload_out);
}

static int manifest_revision_for_read(AttachContext* ctx, const char* config_path,
                                      uint64_t* manifest_rev_out)
{
    if (!manifest_rev_out)
        return -1;
    *manifest_rev_out = 0;
    if (!ctx || !config_path)
        return -1;

    BsAttachStore* store = bs_adapter_attach_ctx_persist_store(ctx);
    if (!store)
        return 0;

    const int reload_rc = bs_adapter_attach_persist_store_reload_manifest(store);
    if (reload_rc != BS_ATTACH_OK)
        return reload_rc;
    return bs_adapter_attach_persist_store_get_revision(store, config_path, manifest_rev_out);
}

static int check_reader_revision_fresh(AttachContext* ctx, const char* config_path,
                                       uint64_t* session_rev_out)
{
    if (!session_rev_out)
        return -1;
    *session_rev_out = bs_adapter_attach_session_path_revision(ctx, config_path);

    uint64_t  manifest_rev = 0;
    const int manifest_rc  = manifest_revision_for_read(ctx, config_path, &manifest_rev);
    if (manifest_rc != 0)
        return manifest_rc;
    if (bs_adapter_attach_ctx_persist_store(ctx) && manifest_rev != *session_rev_out)
        return BS_ATTACH_ERR_REVISION_STALE;
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

    AttachReadGuard guard(ctx);
    if (guard.status() != 0)
        return guard.status();

    size_t total = 0;
    std::shared_ptr<const BsStateSnapshotPayload> payload;
    const int rc = snapshot_pin(ctx, config_path, &payload);
    if (rc == 0)
    {
        if (!payload)
            return -1;
        total = payload->bytes.size();
        uint64_t  session_rev = 0;
        const int fresh_rc    = check_reader_revision_fresh(ctx, config_path, &session_rev);
        if (fresh_rc != 0)
            return fresh_rc;
        out->total_size = total;
        out->revision   = session_rev;
    }
    return rc;
}

int bs_adapter_attach_config_get_snapshot_copy(AttachContext* ctx, const char* config_path,
                                               void* buf, size_t buf_cap, size_t* out_size,
                                               uint64_t* revision_out)
{
    if (!buf || !out_size || !revision_out)
        return -1;

    AttachReadGuard guard(ctx);
    if (guard.status() != 0)
        return guard.status();

    std::shared_ptr<const BsStateSnapshotPayload> payload;
    int rc = snapshot_pin(ctx, config_path, &payload);
    if (rc == 0)
    {
        if (!payload)
            return -1;
        const size_t total = payload->bytes.size();
        uint64_t  session_rev = 0;
        const int fresh_rc    = check_reader_revision_fresh(ctx, config_path, &session_rev);
        if (fresh_rc != 0)
            return fresh_rc;
        *revision_out = session_rev;
        if (total > BS_JSON_MAX_INPUT_BYTES)
            rc = BS_ATTACH_CONC_ERR_TOO_LARGE;
        else if (total > buf_cap)
            rc = -1;
        else
        {
            if (total > 0)
                std::memcpy(buf, payload->bytes.data(), total);
            *out_size = total;
        }
    }
    return rc;
}

int bs_adapter_attach_config_open_snapshot_read(AttachContext* ctx, const char* config_path,
                                                int* handle_out, uint64_t* revision_out)
{
    if (!handle_out || !revision_out)
        return -1;

    std::shared_ptr<const BsStateSnapshotPayload> payload;
    uint64_t                                    rev = 0;

    {
        AttachReadGuard guard(ctx);
        if (guard.status() != 0)
            return guard.status();

        const int rc = snapshot_pin(ctx, config_path, &payload);
        if (rc != 0)
            return rc;
        if (!payload)
            return -1;

        const int fresh_rc = check_reader_revision_fresh(ctx, config_path, &rev);
        if (fresh_rc != 0)
            return fresh_rc;
    }

    auto* st = session_of(ctx);
    if (!st)
        return -1;

    for (auto& h : st->handles)
    {
        if (!h.used)
        {
            h.used        = 1;
            h.path        = config_path ? config_path : "";
            h.revision    = rev;
            h.payload     = payload;
            *handle_out   = static_cast<int>(&h - st->handles);
            *revision_out = rev;
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

    auto* st = session_of(ctx);
    if (!st || handle < 0 || handle >= 16 || !st->handles[handle].used)
        return BS_ATTACH_CONC_ERR_INVALID_HANDLE;

    const SnapshotReadHandle& h = st->handles[handle];

    AttachReadGuard guard(ctx);
    if (guard.status() != 0)
        return guard.status();

    uint64_t  cur      = 0;
    const int fresh_rc = check_reader_revision_fresh(ctx, h.path.c_str(), &cur);
    if (fresh_rc != 0)
        return fresh_rc;
    if (cur != h.revision)
        return BS_ATTACH_CONC_ERR_REVISION_CHANGED;

    const size_t total = h.payload ? h.payload->bytes.size() : 0;
    if (offset >= total)
        return 0;

    const size_t chunk_cap =
        static_cast<size_t>(default_chunk_cap() < buf_cap ? default_chunk_cap() : buf_cap);
    const size_t n      = total - offset;
    const size_t copy_n = n < chunk_cap ? n : chunk_cap;
    if (copy_n > 0 && h.payload)
        std::memcpy(buf, h.payload->bytes.data() + offset, copy_n);
    *out_len = copy_n;
    return 0;
}

void bs_adapter_attach_config_close_snapshot_read(AttachContext* ctx, int handle)
{
    auto* st = session_of(ctx);
    if (!st || handle < 0 || handle >= 16)
        return;
    st->handles[handle].used = 0;
    st->handles[handle].payload.reset();
    st->handles[handle].path.clear();
}

int bs_adapter_attach_config_wait_notify(AttachContext* ctx, const char* config_path,
                                         uint64_t revision_min, int timeout_ms)
{
    auto* st = session_of(ctx);
    if (!st || !config_path)
        return -1;
    if (st->closing.load())
        return BS_ATTACH_CONC_ERR_CLOSED;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    std::unique_lock<std::mutex> lock(st->rev_mu);
    for (;;)
    {
        const auto     it  = st->path_revision.find(config_path);
        const uint64_t rev = it == st->path_revision.end() ? 0 : it->second;
        if (rev >= revision_min)
            return 0;
        if (st->closing.load())
            return BS_ATTACH_CONC_ERR_CLOSED;
        if (timeout_ms < 0)
            st->rev_cv.wait(lock);
        else if (st->rev_cv.wait_until(lock, deadline) == std::cv_status::timeout)
            return BS_ATTACH_CONC_ERR_NOTIFY_TIMEOUT;
    }
}

int bs_adapter_attach_config_read_since_meta(AttachContext* ctx, const char* config_path,
                                             uint64_t revision_min, int timeout_ms,
                                             BsAttachSnapshotMeta* out)
{
    if (!out)
        return -1;
    const int wait_rc =
        bs_adapter_attach_config_wait_notify(ctx, config_path, revision_min, timeout_ms);
    if (wait_rc != 0)
        return wait_rc;
    const int rc = bs_adapter_attach_config_get_snapshot_meta(ctx, config_path, out);
    if (rc == 0 && out->revision < revision_min)
        return BS_ATTACH_CONC_ERR_REVISION_CHANGED;
    return rc;
}
