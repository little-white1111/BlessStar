#include "bs/kernel/common/bs_reentrancy.h"
#include "bs/kernel/ir/ir.h"
#include "bs/kernel/ir/resolver.h"
#include "bs/kernel/report/report.h"

#include "bs/adapter/attach_config.h"
#include "bs/adapter/attach_context.h"
#include "bs/adapter/attach_errors.h"
#include "bs/adapter/attach_execute.h"
#include "bs/adapter/attach_ir_snapshot.h"
#include "bs/adapter/attach_recover_sidecar.h"
#include "bs/adapter/attach_runtime.h"
#include "bs/adapter/attach_session.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/orchestration/reload_gate_default.h"
#include "bs/adapter/parser/config_parse.h"
#include "bs/adapter/persistence/attach_audit.h"
#include "bs/adapter/persistence/attach_store.h"
#include "bs/adapter/persistence/attach_wal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

static uint64_t reload_trace_now_ms()
{
#if defined(_WIN32)
    return static_cast<uint64_t>(GetTickCount64());
#else
    struct timespec ts
    {
    };
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000u + static_cast<uint64_t>(ts.tv_nsec / 1000000u);
#endif
}

static int reload_trace_enabled()
{
    static int cached = -1;
    if (cached < 0)
    {
        const char* env = std::getenv("BS_ATTACH_RELOAD_TRACE");
        cached          = (env && env[0] == '1') ? 1 : 0;
    }
    return cached;
}

static void reload_trace(const char* phase, const char* detail = nullptr)
{
    if (!reload_trace_enabled())
        return;
    static const uint64_t t0      = reload_trace_now_ms();
    const uint64_t        elapsed = reload_trace_now_ms() - t0;
    if (detail && detail[0])
        std::fprintf(stderr, "[reload_trace] +%llums %s %s\n",
                     static_cast<unsigned long long>(elapsed), phase, detail);
    else
        std::fprintf(stderr, "[reload_trace] +%llums %s\n",
                     static_cast<unsigned long long>(elapsed), phase);
    (void)std::fflush(stderr);
}

struct PathWork
{
    std::string              uri;
    PathOrchestrationState   state         = BS_ORCH_PENDING;
    uint64_t                 base_revision = 0;
    std::vector<uint8_t>     staged_payload;
    IRInstructionList*       gated_ir           = nullptr;
    BsAttachIrSnapshotHandle ir_snapshot_handle = 0;
};

struct ReloadBatchController
{
    unsigned                                max_inflight = 8;
    unsigned                                max_retry    = 0;
    std::vector<PathWork>                   paths;
    BatchOutcome                            outcome  = BATCH_ALL_OK;
    ReloadPathReadFn                        read_fn  = nullptr;
    void*                                   read_ctx = nullptr;
    ReloadPathGateFn                        gate_fn  = nullptr;
    void*                                   gate_ctx = nullptr;
    Report*                                 report   = nullptr;
    std::unordered_map<std::string, size_t> uri_index;

    BsAttachScheme scheme = BS_ATTACH_SCHEME_UNSET;
    std::string    manifest_path;
    size_t         session_memory_cap = BS_ATTACH_SESSION_MEMORY_CAP_DEFAULT;
    size_t         session_bytes_used = 0;
    BsAttachStore* attach_store       = nullptr;
    AttachContext* attach_ctx         = nullptr;
    int            store_owned        = 0; /* 1 if controller opened store (no ctx persist_store) */
};

#if defined(BS_TESTING)
static int g_testing_abort_after_exec = 0;
#endif

static int      ensure_attach_store(ReloadBatchController* ctrl);
static uint64_t session_batch_epoch(const ReloadBatchController* ctrl);

static uint32_t uri_set_hash_for_ctrl(const ReloadBatchController* ctrl)
{
    if (!ctrl)
        return 0;
    std::vector<std::string> uris;
    uris.reserve(ctrl->paths.size());
    for (const auto& w : ctrl->paths)
        uris.push_back(w.uri);
    std::sort(uris.begin(), uris.end());
    std::string blob;
    for (const auto& u : uris)
    {
        blob.append(u);
        blob.push_back('\0');
    }
    if (blob.empty())
        return 0;
    uint32_t h = 2166136261u;
    for (unsigned char c : blob)
    {
        h ^= (uint32_t)c;
        h *= 16777619u;
    }
    return h;
}

static uint64_t pending_batch_epoch(const ReloadBatchController* ctrl)
{
    return session_batch_epoch(ctrl) + 1;
}

static int write_phase_mark(ReloadBatchController* ctrl, BsAttachWalRecoverPhase phase)
{
    if (!ctrl || ctrl->scheme != BS_ATTACH_SCHEME_PER_BATCH)
        return BS_ATTACH_OK;
    if (ensure_attach_store(ctrl) != 0)
        return BS_ATTACH_ERR_IO;
    return bs_adapter_attach_persist_store_append_phase_mark(
        ctrl->attach_store, pending_batch_epoch(ctrl), (uint32_t)phase,
        uri_set_hash_for_ctrl(ctrl));
}

static AttachContext* resolve_attach_ctx(ReloadBatchController* ctrl)
{
    if (ctrl && ctrl->attach_ctx)
        return ctrl->attach_ctx;
    AttachActiveGuard guard;
    return bs_adapter_attach_ctx_get_active();
}

static void report_audit_failure(ReloadBatchController* ctrl, const char* uri, const char* stage,
                                 int abort_code, const char* detail);

ReloadBatchController* bs_adapter_attach_reload_batch_create(unsigned max_inflight)
{
    auto* c = new ReloadBatchController();
    if (max_inflight > 0)
        c->max_inflight = max_inflight;
    return c;
}

static void release_path_work_ir(PathWork* w)
{
    if (!w)
        return;
    if (w->gated_ir)
    {
        bs_ir_instruction_list_destroy(w->gated_ir);
        w->gated_ir = nullptr;
    }
    w->staged_payload.clear();
    w->ir_snapshot_handle = 0;
    w->state              = BS_ORCH_PENDING;
    w->base_revision      = 0;
}

void bs_adapter_attach_reload_batch_reset(ReloadBatchController* ctrl)
{
    if (!ctrl)
        return;

    AttachContext* actx = resolve_attach_ctx(ctrl);
    if (actx && bs_adapter_attach_session_in_write_window(actx))
        bs_adapter_attach_session_end_write_window(actx);

    if (ctrl->attach_store && ctrl->scheme == BS_ATTACH_SCHEME_PER_BATCH)
        bs_adapter_attach_persist_store_batch_abort(ctrl->attach_store);

    for (auto& w : ctrl->paths)
        release_path_work_ir(&w);
    ctrl->paths.clear();
    ctrl->uri_index.clear();
    ctrl->session_bytes_used = 0;
    ctrl->outcome            = BATCH_ALL_OK;

    if (actx)
        bs_adapter_attach_ir_snapshot_clear_all(actx);
}

void bs_adapter_attach_reload_batch_destroy(ReloadBatchController* ctrl)
{
    if (!ctrl)
        return;

    bs_adapter_attach_reload_batch_reset(ctrl);

    if (ctrl->store_owned)
        bs_adapter_attach_persist_store_close(ctrl->attach_store);
    ctrl->attach_store = nullptr;
    ctrl->store_owned  = 0;
    delete ctrl;
}

void bs_adapter_attach_reload_batch_set_read_fn(ReloadBatchController* ctrl, ReloadPathReadFn fn,
                                                void* user_ctx)
{
    if (!ctrl)
        return;
    ctrl->read_fn  = fn;
    ctrl->read_ctx = user_ctx;
}

void bs_adapter_attach_reload_batch_set_gate_fn(ReloadBatchController* ctrl, ReloadPathGateFn fn,
                                                void* user_ctx)
{
    if (!ctrl)
        return;
    ctrl->gate_fn  = fn;
    ctrl->gate_ctx = user_ctx;
}

void bs_adapter_attach_reload_batch_set_attach_ctx(ReloadBatchController* ctrl, AttachContext* ctx)
{
    if (!ctrl)
        return;
    ctrl->attach_ctx = ctx;
}

void bs_adapter_attach_reload_batch_set_default_gate(ReloadBatchController* ctrl)
{
    bs_adapter_attach_reload_batch_set_gate_fn(ctrl, bs_adapter_attach_reload_default_path_gate,
                                               NULL);
}

void bs_adapter_attach_reload_batch_set_max_retry(ReloadBatchController* ctrl, unsigned max_retry)
{
    if (ctrl)
        ctrl->max_retry = max_retry;
}

void bs_adapter_attach_reload_batch_set_report(ReloadBatchController* ctrl, Report* report)
{
    if (ctrl)
        ctrl->report = report;
}

int bs_adapter_attach_reload_batch_set_attach_scheme(ReloadBatchController* ctrl,
                                                     BsAttachScheme         scheme)
{
    if (!ctrl)
        return BS_ATTACH_ERR_INVALID_ARG;
    if (scheme != BS_ATTACH_SCHEME_PER_PATH && scheme != BS_ATTACH_SCHEME_PER_BATCH)
        return BS_ATTACH_ERR_INVALID_ARG;
    ctrl->scheme = scheme;
    return BS_ATTACH_OK;
}

void bs_adapter_attach_reload_batch_set_manifest_path(ReloadBatchController* ctrl,
                                                      const char*            manifest_path)
{
    if (!ctrl)
        return;
    if (manifest_path)
        ctrl->manifest_path = manifest_path;
    else
        ctrl->manifest_path.clear();
}

void bs_adapter_attach_reload_batch_set_session_memory_cap(ReloadBatchController* ctrl,
                                                           size_t                 cap_bytes)
{
    if (!ctrl)
        return;
    ctrl->session_memory_cap = (cap_bytes == 0) ? BS_ATTACH_SESSION_MEMORY_CAP_DEFAULT : cap_bytes;
}

int bs_adapter_attach_reload_batch_add_path(ReloadBatchController* ctrl, const char* uri)
{
    if (!ctrl || !uri || uri[0] == '\0')
        return -1;
    if (ctrl->paths.size() >= ctrl->max_inflight)
        return -1;

    PathWork w;
    w.uri                  = uri;
    ctrl->uri_index[w.uri] = ctrl->paths.size();
    ctrl->paths.push_back(std::move(w));
    return 0;
}

static void gc_path_work(PathWork* w)
{
    if (!w)
        return;
    if (w->gated_ir)
    {
        bs_ir_instruction_list_destroy(w->gated_ir);
        w->gated_ir = nullptr;
    }
    w->staged_payload.clear();
    /* Keep terminal path state for bs_adapter_attach_reload_batch_path_state (XV-IO-02). */
}

static int gate_path_work(ReloadBatchController* ctrl, PathWork* w, IoReadResult* result,
                          BsReloadGateDetail* detail)
{
    AttachContext* actx       = resolve_attach_ctx(ctrl);
    const int      pool_ready = actx && bs_adapter_attach_ctx_is_kernel_pool_warmed(actx);

    if (!pool_ready)
    {
        if (!ctrl->gate_fn)
            return bs_adapter_attach_reload_default_path_gate(nullptr, w->uri.c_str(), result,
                                                              detail);
        return ctrl->gate_fn(ctrl->gate_ctx, w->uri.c_str(), result, detail);
    }

    BsConfigParseResult parsed{};
    const int parse_rc = bs_adapter_attach_reload_parse_and_verify_bytes(result, &parsed, detail);
    if (parse_rc != BS_RELOAD_GATE_OK)
    {
        bs_adapter_parser_result_destroy(&parsed);
        return parse_rc;
    }

    if (ctrl->gate_fn && ctrl->gate_fn != bs_adapter_attach_reload_default_path_gate)
    {
        const int gate_rc = ctrl->gate_fn(ctrl->gate_ctx, w->uri.c_str(), result, detail);
        if (gate_rc != BS_RELOAD_GATE_OK)
        {
            bs_adapter_parser_result_destroy(&parsed);
            return gate_rc;
        }
    }

    w->gated_ir         = parsed.instructions;
    parsed.instructions = nullptr;
    if (parsed.active_requirements)
    {
        bs_requirement_list_free(parsed.active_requirements);
        parsed.active_requirements = nullptr;
    }
    return BS_RELOAD_GATE_OK;
}

static int publish_path_ir(AttachContext* actx, PathWork* w)
{
    if (!actx || !w->gated_ir)
        return -1;
    w->ir_snapshot_handle =
        bs_adapter_attach_ir_snapshot_publish(actx, w->uri.c_str(), w->base_revision, w->gated_ir);
    w->gated_ir = nullptr;
    return w->ir_snapshot_handle ? 0 : -1;
}

static int exec_path_ir(AttachContext* actx, PathWork* w, ReloadBatchController* ctrl)
{
    if (!actx || w->ir_snapshot_handle == 0)
        return -1;

    w->state = BS_ORCH_EXECUTING;
    bs_adapter_attach_ir_snapshot_pin(actx, w->ir_snapshot_handle);
    IRInstructionList* instructions =
        bs_adapter_attach_ir_snapshot_instructions(actx, w->ir_snapshot_handle);
    Report*   report  = nullptr;
    const int exec_rc = bs_adapter_attach_exec_parsed_ir(actx, instructions, &report);
    bs_adapter_attach_ir_snapshot_unpin(actx, w->ir_snapshot_handle);
    if (report)
        bs_report_destroy(report);
    if (exec_rc != 0)
    {
        w->state      = BS_ORCH_EXEC_REJECTED;
        ctrl->outcome = BATCH_COMPLETED_WITH_FAILURES;
        report_audit_failure(ctrl, w->uri.c_str(), "ir_execute", exec_rc, "exec rejected");
        return exec_rc;
    }
    if (bs_adapter_attach_ir_snapshot_remove(actx, w->ir_snapshot_handle) != 0)
    {
        w->state      = BS_ORCH_EXEC_REJECTED;
        ctrl->outcome = BATCH_COMPLETED_WITH_FAILURES;
        report_audit_failure(ctrl, w->uri.c_str(), "ir_snapshot", -1, "snapshot remove failed");
        return -1;
    }
    w->ir_snapshot_handle = 0;
    w->state              = BS_ORCH_STAGED;
    return 0;
}

static int run_read_with_retry(ReloadBatchController* ctrl, const char* uri, IoReadResult* out)
{
    unsigned attempt = 0;
    for (;;)
    {
        const int rc = ctrl->read_fn(ctrl->read_ctx, uri, out);
        if (rc == BS_IO_OK)
            return BS_IO_OK;
        if (attempt >= ctrl->max_retry)
            return rc;
        bs_io_read_result_free(out);
        bs_io_read_result_init(out);
        ++attempt;
    }
}

static uint64_t path_base_revision(const ReloadBatchController* ctrl, const char* uri)
{
    if (!ctrl || !uri)
        return 0;
    const auto it = ctrl->uri_index.find(uri);
    if (it == ctrl->uri_index.end())
        return 0;
    return ctrl->paths[it->second].base_revision;
}

static uint64_t session_batch_epoch(const ReloadBatchController* ctrl)
{
    return (ctrl && ctrl->attach_store)
               ? bs_adapter_attach_persist_store_batch_epoch(ctrl->attach_store)
               : 0;
}

static void report_audit_failure(ReloadBatchController* ctrl, const char* uri, const char* stage,
                                 int abort_code, const char* detail)
{
    if (!ctrl || !ctrl->report || !stage)
        return;
    bs_adapter_attach_persist_report_audit(ctrl->report, stage, ctrl->scheme,
                                           session_batch_epoch(ctrl), uri,
                                           path_base_revision(ctrl, uri), abort_code, detail);
}

static int ensure_attach_store(ReloadBatchController* ctrl)
{
    if (!ctrl || ctrl->attach_store)
        return ctrl && ctrl->attach_store ? 0 : -1;

    AttachContext* actx = resolve_attach_ctx(ctrl);
    if (actx)
    {
        BsAttachStore* ctx_store = bs_adapter_attach_ctx_persist_store(actx);
        if (!ctx_store && !ctrl->manifest_path.empty())
        {
            if (bs_adapter_attach_ctx_open_persist_store(actx, ctrl->manifest_path.c_str()) != 0)
                return -1;
            ctx_store = bs_adapter_attach_ctx_persist_store(actx);
        }
        if (ctx_store)
        {
            ctrl->attach_store = ctx_store;
            return 0;
        }
    }

    const char* path   = ctrl->manifest_path.empty() ? nullptr : ctrl->manifest_path.c_str();
    ctrl->attach_store = bs_adapter_attach_persist_store_open(path);
    if (!ctrl->attach_store)
        return -1;
    ctrl->store_owned = 1;
    return 0;
}

static int load_session_revisions(ReloadBatchController* ctrl)
{
    if (ensure_attach_store(ctrl) != 0)
        return -1;
    if (bs_adapter_attach_persist_store_reload_manifest(ctrl->attach_store) != BS_ATTACH_OK)
        return -1;
    for (auto& w : ctrl->paths)
    {
        if (bs_adapter_attach_persist_store_get_revision(ctrl->attach_store, w.uri.c_str(),
                                                         &w.base_revision) != BS_ATTACH_OK)
            return -1;
    }
    return 0;
}

static int account_session_bytes(ReloadBatchController* ctrl, size_t nbytes)
{
    if (nbytes > ctrl->session_memory_cap ||
        nbytes > ctrl->session_memory_cap - ctrl->session_bytes_used)
        return BS_ATTACH_ERR_LIMIT;
    ctrl->session_bytes_used += nbytes;
    return BS_ATTACH_OK;
}

static const char* attach_err_detail(int rc)
{
    switch (rc)
    {
    case BS_ATTACH_ERR_CONFLICT:
        return "revision conflict";
    case BS_ATTACH_ERR_OOM:
        return "oom";
    case BS_ATTACH_ERR_IO:
        return "persist io failed";
    case BS_ATTACH_ERR_LIMIT:
        return "session memory cap exceeded";
    default:
        return "persist failed";
    }
}

static int persist_per_path(ReloadBatchController* ctrl, PathWork* w, const IoReadResult* result)
{
    const int rc = bs_adapter_attach_persist_store_commit_per_path(
        ctrl->attach_store, w->uri.c_str(), result->data, result->length, w->base_revision);
    if (rc != BS_ATTACH_OK)
    {
        w->state = BS_ORCH_PERSIST_REJECTED;
        report_audit_failure(ctrl, w->uri.c_str(), "persistent_commit", rc, attach_err_detail(rc));
        return rc;
    }
    w->state = BS_ORCH_COMMITTED;
    if (ctrl->report)
    {
        uint64_t new_rev = 0;
        bs_adapter_attach_persist_store_get_revision(ctrl->attach_store, w->uri.c_str(), &new_rev);
        bs_adapter_attach_persist_report_persist_ok(
            ctrl->report, ctrl->scheme, session_batch_epoch(ctrl), w->uri.c_str(), new_rev);
    }
    AttachContext* actx = resolve_attach_ctx(ctrl);
    if (actx && bs_adapter_attach_config_has_manager(actx) && result && result->data &&
        result->length > 0)
    {
        const int sync_rc =
            bs_adapter_attach_config_sync_path(actx, w->uri.c_str(), result->data, result->length);
        if (sync_rc != 0)
        {
            w->state = BS_ORCH_PERSIST_REJECTED;
            report_audit_failure(ctrl, w->uri.c_str(), "config_sync", sync_rc,
                                 "config manager sync failed");
            return sync_rc;
        }
        const int post_rc =
            bs_adapter_attach_post_config_sync(actx, w->uri.c_str(), ctrl->attach_store);
        if (post_rc != 0)
        {
            w->state = BS_ORCH_PERSIST_REJECTED;
            report_audit_failure(ctrl, w->uri.c_str(), "config_sync", post_rc,
                                 "post config sync failed");
            return post_rc;
        }
    }
    return BS_ATTACH_OK;
}

int bs_adapter_attach_reload_batch_run(ReloadBatchController* ctrl)
{
    if (!ctrl || !ctrl->read_fn)
        return -1;

    if (ctrl->scheme == BS_ATTACH_SCHEME_UNSET)
        return -4;

    if (bs_reentrancy_in_state_callback())
        return BS_ATTACH_CONC_ERR_REENTRANT;

    AttachContext* actx = resolve_attach_ctx(ctrl);
    if (!actx || !bs_adapter_attach_ctx_is_log_bus_bound(actx))
        return -2;

    reload_trace("run_start",
                 ctrl->scheme == BS_ATTACH_SCHEME_PER_BATCH ? "PER_BATCH" : "PER_PATH");

    int  write_window_open        = 0;
    auto end_write_window_if_open = [&]()
    {
        if (write_window_open && actx)
        {
            bs_adapter_attach_session_end_write_window(actx);
            write_window_open = 0;
        }
    };
    auto begin_write_window = [&]()
    {
        if (!write_window_open && actx)
        {
            bs_adapter_attach_session_begin_write_window(actx);
            write_window_open = 1;
        }
    };

    reload_trace("begin_write_window");
    begin_write_window();
    reload_trace("begin_write_window_done");

    if (!ctrl->gate_fn)
        bs_adapter_attach_reload_batch_set_default_gate(ctrl);

    if (load_session_revisions(ctrl) != 0)
    {
        end_write_window_if_open();
        return -1;
    }
    reload_trace("load_session_revisions");

    if (!ctrl->manifest_path.empty())
    {
        reload_trace("recover_sidecar_begin");
        (void)bs_adapter_attach_recover_sidecar_invalidate(ctrl->manifest_path.c_str());
        reload_trace("recover_sidecar_done");
    }

    ctrl->outcome            = BATCH_ALL_OK;
    ctrl->session_bytes_used = 0;

    if (ctrl->scheme == BS_ATTACH_SCHEME_PER_BATCH)
    {
        bs_adapter_attach_persist_store_batch_begin(ctrl->attach_store);
        if (write_phase_mark(ctrl, BS_ATTACH_WAL_PHASE_STAGE) != BS_ATTACH_OK)
        {
            end_write_window_if_open();
            return BS_ATTACH_ERR_IO;
        }
    }

    if (ctrl->report)
    {
        reload_trace("report_session_begin");
        const uint64_t epoch = session_batch_epoch(ctrl);
        for (const auto& w : ctrl->paths)
        {
            bs_adapter_attach_persist_report_session_begin(ctrl->report, ctrl->scheme, epoch,
                                                           w.uri.c_str(), w.base_revision);
        }
        reload_trace("report_session_done");
    }

    reload_trace("path_loop_begin");
    bool batch_had_failure = false;

    for (auto& w : ctrl->paths)
    {
        w.state = BS_ORCH_READING;
        reload_trace("path_read_begin", w.uri.c_str());

        IoReadResult result{};
        const int    read_rc = run_read_with_retry(ctrl, w.uri.c_str(), &result);
        if (read_rc != BS_IO_OK)
        {
            w.state           = BS_ORCH_FAILED_READ;
            ctrl->outcome     = BATCH_COMPLETED_WITH_FAILURES;
            batch_had_failure = true;
            report_audit_failure(ctrl, w.uri.c_str(), "cache_attach", read_rc,
                                 result.error_message ? result.error_message : "read failed");
            bs_io_read_result_free(&result);
            gc_path_work(&w);
            continue;
        }

        if (account_session_bytes(ctrl, result.length) != BS_ATTACH_OK)
        {
            w.state           = BS_ORCH_FAILED_READ;
            ctrl->outcome     = BATCH_COMPLETED_WITH_FAILURES;
            batch_had_failure = true;
            report_audit_failure(ctrl, w.uri.c_str(), "cache_attach", BS_ATTACH_ERR_LIMIT,
                                 "session memory cap exceeded");
            bs_io_read_result_free(&result);
            gc_path_work(&w);
            continue;
        }

        reload_trace("path_read_done", w.uri.c_str());
        w.state = BS_ORCH_GATING;
        BsReloadGateDetail gate_detail{};
        const int          gate_rc = gate_path_work(ctrl, &w, &result, &gate_detail);
        reload_trace("path_gate_done", w.uri.c_str());
        if (gate_rc != BS_RELOAD_GATE_OK)
        {
            w.state            = BS_ORCH_GATE_REJECTED;
            ctrl->outcome      = BATCH_COMPLETED_WITH_FAILURES;
            batch_had_failure  = true;
            const char* stage  = (gate_rc == BS_RELOAD_GATE_PARSE_FAIL) ? "parse" : "cache_attach";
            const char* detail = gate_detail.buf[0]                       ? gate_detail.buf
                                 : (gate_rc == BS_RELOAD_GATE_PARSE_FAIL) ? "parse failed"
                                                                          : "gate rejected";
            report_audit_failure(ctrl, w.uri.c_str(), stage, gate_rc, detail);
            bs_io_read_result_free(&result);
            gc_path_work(&w);
            continue;
        }

        const int pool_ready = actx && bs_adapter_attach_ctx_is_kernel_pool_warmed(actx);
        if (pool_ready && publish_path_ir(actx, &w) != 0)
        {
            w.state           = BS_ORCH_GATE_REJECTED;
            ctrl->outcome     = BATCH_COMPLETED_WITH_FAILURES;
            batch_had_failure = true;
            report_audit_failure(ctrl, w.uri.c_str(), "ir_snapshot", -1, "snapshot publish failed");
            bs_io_read_result_free(&result);
            gc_path_work(&w);
            continue;
        }

        if (ctrl->scheme == BS_ATTACH_SCHEME_PER_PATH)
        {
            if (pool_ready)
            {
                /* P1: pool exec must not run under session write-window (matches PER_BATCH). */
                end_write_window_if_open();
                reload_trace("path_exec_begin", w.uri.c_str());
                if (exec_path_ir(actx, &w, ctrl) != 0)
                {
                    batch_had_failure = true;
                    bs_io_read_result_free(&result);
                    gc_path_work(&w);
                    continue;
                }
                reload_trace("path_exec_done", w.uri.c_str());
                /* post_config_sync resets kernel pool pipelines; keep write-window closed. */
            }
            reload_trace("path_persist_begin", w.uri.c_str());
            if (persist_per_path(ctrl, &w, &result) != BS_ATTACH_OK)
                ctrl->outcome = BATCH_COMPLETED_WITH_FAILURES;
            bs_io_read_result_free(&result);
            gc_path_work(&w);
            continue;
        }

        w.state = BS_ORCH_STAGED;
        w.staged_payload.assign(result.data, result.data + result.length);
        const int st = bs_adapter_attach_persist_store_batch_stage(
            ctrl->attach_store, w.uri.c_str(), result.data, result.length, w.base_revision);
        bs_io_read_result_free(&result);
        if (st != BS_ATTACH_OK)
        {
            w.state           = BS_ORCH_PERSIST_REJECTED;
            ctrl->outcome     = BATCH_COMPLETED_WITH_FAILURES;
            batch_had_failure = true;
            report_audit_failure(ctrl, w.uri.c_str(), "persistent_commit", st,
                                 attach_err_detail(st));
            gc_path_work(&w);
        }
    }

    if (ctrl->scheme == BS_ATTACH_SCHEME_PER_BATCH && !batch_had_failure)
    {
        const bool has_staged =
            std::any_of(ctrl->paths.begin(), ctrl->paths.end(),
                        [](const PathWork& w) { return w.state == BS_ORCH_STAGED; });
        if (has_staged && write_phase_mark(ctrl, BS_ATTACH_WAL_PHASE_GATE) != BS_ATTACH_OK)
        {
            end_write_window_if_open();
            return BS_ATTACH_ERR_IO;
        }
    }

    if (ctrl->scheme == BS_ATTACH_SCHEME_PER_BATCH && !batch_had_failure)
    {
        const bool pool_parallel =
            actx && bs_adapter_attach_ctx_is_kernel_pool_warmed(actx) &&
            std::any_of(ctrl->paths.begin(), ctrl->paths.end(),
                        [](const PathWork& w) { return w.state == BS_ORCH_STAGED; });
        if (pool_parallel)
        {
            /* P1: pool exec must not run under session write-window (session_mu exclusive).
             * Keep PER_BATCH execution on the caller thread: Windows CI exposed a wait-chain
             * deadlock when temporary workers nested kernel-pool submit/executor waits. */
            end_write_window_if_open();
            reload_trace("batch_pool_exec_begin");

            bool exec_failed = false;
            for (auto& w : ctrl->paths)
            {
                if (w.state != BS_ORCH_STAGED)
                    continue;
                reload_trace("path_exec_begin", w.uri.c_str());
                if (exec_path_ir(actx, &w, ctrl) != 0)
                    exec_failed = true;
                reload_trace("path_exec_done", w.uri.c_str());
            }
            reload_trace("batch_pool_exec_done");
            if (exec_failed)
            {
                batch_had_failure = true;
                ctrl->outcome     = BATCH_COMPLETED_WITH_FAILURES;
                bs_adapter_attach_ir_snapshot_clear_all(actx);
            }
            else
            {
                if (write_phase_mark(ctrl, BS_ATTACH_WAL_PHASE_EXEC) != BS_ATTACH_OK)
                {
                    batch_had_failure = true;
                    ctrl->outcome     = BATCH_COMPLETED_WITH_FAILURES;
                }
#if defined(BS_TESTING)
                else if (g_testing_abort_after_exec)
                {
                    batch_had_failure = true;
                    ctrl->outcome     = BATCH_COMPLETED_WITH_FAILURES;
                    bs_adapter_attach_ir_snapshot_clear_all(actx);
                    bs_adapter_attach_persist_store_batch_abort(ctrl->attach_store);
                    end_write_window_if_open();
                    return BS_ATTACH_ERR_IO;
                }
#endif
            }
        }
    }

    if (ctrl->scheme == BS_ATTACH_SCHEME_PER_BATCH)
    {
        if (batch_had_failure)
        {
            bs_adapter_attach_persist_store_batch_abort(ctrl->attach_store);
            report_audit_failure(ctrl, "batch", "persistent_commit", BS_ATTACH_ERR_IO,
                                 "batch_aborted");
            for (auto& w : ctrl->paths)
            {
                if (w.state == BS_ORCH_STAGED)
                {
                    w.state = BS_ORCH_PERSIST_REJECTED;
                    gc_path_work(&w);
                }
            }
        }
        else if (!ctrl->paths.empty())
        {
            begin_write_window();
            if (write_phase_mark(ctrl, BS_ATTACH_WAL_PHASE_PERSIST) != BS_ATTACH_OK)
            {
                ctrl->outcome = BATCH_COMPLETED_WITH_FAILURES;
                bs_adapter_attach_persist_store_batch_abort(ctrl->attach_store);
                end_write_window_if_open();
                return BS_ATTACH_ERR_IO;
            }
            const int bc = bs_adapter_attach_persist_store_batch_commit(ctrl->attach_store);
            /* Pool reset in post_config_sync must not run under write-window (matches PER_PATH). */
            end_write_window_if_open();
            if (bc != BS_ATTACH_OK)
            {
                ctrl->outcome = BATCH_COMPLETED_WITH_FAILURES;
                report_audit_failure(ctrl, "batch", "persistent_commit", bc, attach_err_detail(bc));
                for (auto& w : ctrl->paths)
                {
                    if (w.state == BS_ORCH_STAGED)
                        w.state = BS_ORCH_PERSIST_REJECTED;
                    gc_path_work(&w);
                }
            }
            else
            {
                const uint64_t epoch = session_batch_epoch(ctrl);
                for (auto& w : ctrl->paths)
                {
                    if (w.state == BS_ORCH_STAGED)
                    {
                        if (actx && bs_adapter_attach_config_has_manager(actx) &&
                            !w.staged_payload.empty())
                        {
                            const int sync_rc = bs_adapter_attach_config_sync_path(
                                actx, w.uri.c_str(), w.staged_payload.data(),
                                w.staged_payload.size());
                            if (sync_rc != 0)
                            {
                                w.state           = BS_ORCH_PERSIST_REJECTED;
                                ctrl->outcome     = BATCH_COMPLETED_WITH_FAILURES;
                                batch_had_failure = true;
                                report_audit_failure(ctrl, w.uri.c_str(), "config_sync", sync_rc,
                                                     "config manager sync failed");
                                gc_path_work(&w);
                                continue;
                            }
                            const int post_rc = bs_adapter_attach_post_config_sync(
                                actx, w.uri.c_str(), ctrl->attach_store);
                            if (post_rc != 0)
                            {
                                w.state           = BS_ORCH_PERSIST_REJECTED;
                                ctrl->outcome     = BATCH_COMPLETED_WITH_FAILURES;
                                batch_had_failure = true;
                                report_audit_failure(ctrl, w.uri.c_str(), "config_sync", post_rc,
                                                     "post config sync failed");
                                gc_path_work(&w);
                                continue;
                            }
                        }
                        w.state = BS_ORCH_COMMITTED;
                        if (ctrl->report)
                        {
                            uint64_t new_rev = 0;
                            bs_adapter_attach_persist_store_get_revision(ctrl->attach_store,
                                                                         w.uri.c_str(), &new_rev);
                            bs_adapter_attach_persist_report_persist_ok(
                                ctrl->report, ctrl->scheme, epoch, w.uri.c_str(), new_rev);
                        }
                    }
                    gc_path_work(&w);
                }
            }
        }
        else
        {
            bs_adapter_attach_persist_store_batch_abort(ctrl->attach_store);
        }
    }

    end_write_window_if_open();
    /* PER_PATH pool exec closes write window before persist; sync enqueues watch jobs
     * outside the window, so drain pending notifications before returning. */
    if (actx)
    {
        reload_trace("drain_notifications_begin");
        bs_adapter_attach_session_drain_pending_notifications(actx);
        reload_trace("drain_notifications_done");
    }
    reload_trace("run_end");
    return 0;
}

BatchOutcome bs_adapter_attach_reload_batch_outcome(const ReloadBatchController* ctrl)
{
    return ctrl ? ctrl->outcome : BATCH_COMPLETED_WITH_FAILURES;
}

PathOrchestrationState bs_adapter_attach_reload_batch_path_state(const ReloadBatchController* ctrl,
                                                                 const char*                  uri)
{
    if (!ctrl || !uri)
        return BS_ORCH_PENDING;
    const auto it = ctrl->uri_index.find(uri);
    if (it == ctrl->uri_index.end())
        return BS_ORCH_PENDING;
    return ctrl->paths[it->second].state;
}

#if defined(BS_TESTING)
void bs_adapter_attach_reload_batch_testing_set_abort_after_exec(int enabled)
{
    g_testing_abort_after_exec = enabled ? 1 : 0;
}
#endif
