#include "bs/kernel/common/bs_reentrancy.h"
#include "bs/kernel/ir/ir.h"
#include "bs/kernel/ir/resolver.h"
#include "bs/kernel/report/report.h"

#include "bs/adapter/attach_config.h"
#include "bs/adapter/attach_context.h"
#include "bs/adapter/attach_errors.h"
#include "bs/adapter/attach_execute.h"
#include "bs/adapter/attach_ir_snapshot.h"
#include "bs/adapter/attach_runtime.h"
#include "bs/adapter/attach_session.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/orchestration/reload_gate_default.h"
#include "bs/adapter/parser/config_parse.h"
#include "bs/adapter/persistence/attach_audit.h"
#include "bs/adapter/persistence/attach_store.h"

#include <cstdio>
#include <cstring>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

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
};

static void report_audit_failure(ReloadBatchController* ctrl, const char* uri, const char* stage,
                                 int abort_code, const char* detail);

ReloadBatchController* bs_adapter_attach_reload_batch_create(unsigned max_inflight)
{
    auto* c = new ReloadBatchController();
    if (max_inflight > 0)
        c->max_inflight = max_inflight;
    return c;
}

void bs_adapter_attach_reload_batch_destroy(ReloadBatchController* ctrl)
{
    if (!ctrl)
        return;
    for (auto& w : ctrl->paths)
    {
        if (w.gated_ir)
        {
            bs_ir_instruction_list_destroy(w.gated_ir);
            w.gated_ir = nullptr;
        }
    }
    bs_adapter_attach_persist_store_close(ctrl->attach_store);
    ctrl->attach_store = nullptr;
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
    AttachContext* actx       = bs_adapter_attach_ctx_get_active();
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
    w->state = BS_ORCH_STAGED;
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
    if (ctrl->attach_store)
        return 0;
    const char* path   = ctrl->manifest_path.empty() ? nullptr : ctrl->manifest_path.c_str();
    ctrl->attach_store = bs_adapter_attach_persist_store_open(path);
    return ctrl->attach_store ? 0 : -1;
}

static int load_session_revisions(ReloadBatchController* ctrl)
{
    if (ensure_attach_store(ctrl) != 0)
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
    AttachContext* actx = bs_adapter_attach_ctx_get_active();
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
    {
        bs_reentrancy_trap_listener_write_violation();
        return BS_ATTACH_CONC_ERR_REENTRANT;
    }

    AttachActiveGuard active_guard;
    if (!bs_adapter_attach_is_log_ready())
        return -2;

    AttachContext* actx_for_window = bs_adapter_attach_ctx_get_active();
    if (actx_for_window)
        bs_adapter_attach_session_begin_write_window(actx_for_window);

    if (!ctrl->gate_fn)
        bs_adapter_attach_reload_batch_set_default_gate(ctrl);

    if (load_session_revisions(ctrl) != 0)
    {
        if (actx_for_window)
            bs_adapter_attach_session_end_write_window(actx_for_window);
        return -1;
    }

    ctrl->outcome            = BATCH_ALL_OK;
    ctrl->session_bytes_used = 0;

    if (ctrl->scheme == BS_ATTACH_SCHEME_PER_BATCH)
        bs_adapter_attach_persist_store_batch_begin(ctrl->attach_store);

    if (ctrl->report)
    {
        const uint64_t epoch = session_batch_epoch(ctrl);
        for (const auto& w : ctrl->paths)
        {
            bs_adapter_attach_persist_report_session_begin(ctrl->report, ctrl->scheme, epoch,
                                                           w.uri.c_str(), w.base_revision);
        }
    }

    bool batch_had_failure = false;

    for (auto& w : ctrl->paths)
    {
        w.state = BS_ORCH_READING;

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

        w.state = BS_ORCH_GATING;
        BsReloadGateDetail gate_detail{};
        const int          gate_rc = gate_path_work(ctrl, &w, &result, &gate_detail);
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

        AttachContext* actx       = bs_adapter_attach_ctx_get_active();
        const int      pool_ready = actx && bs_adapter_attach_ctx_is_kernel_pool_warmed(actx);
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
            if (pool_ready && exec_path_ir(actx, &w, ctrl) != 0)
            {
                batch_had_failure = true;
                bs_io_read_result_free(&result);
                gc_path_work(&w);
                continue;
            }
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
        AttachContext* actx = bs_adapter_attach_ctx_get_active();
        if (actx && bs_adapter_attach_ctx_is_kernel_pool_warmed(actx))
        {
            std::vector<std::thread> workers;
            std::mutex               fail_mu;
            bool                     exec_failed = false;
            for (auto& w : ctrl->paths)
            {
                if (w.state != BS_ORCH_STAGED)
                    continue;
                workers.emplace_back(
                    [&w, actx, ctrl, &fail_mu, &exec_failed]()
                    {
                        if (exec_path_ir(actx, &w, ctrl) != 0)
                        {
                            std::lock_guard<std::mutex> lock(fail_mu);
                            exec_failed = true;
                        }
                    });
            }
            for (auto& worker : workers)
                worker.join();
            if (exec_failed)
            {
                batch_had_failure = true;
                ctrl->outcome     = BATCH_COMPLETED_WITH_FAILURES;
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
            const int bc = bs_adapter_attach_persist_store_batch_commit(ctrl->attach_store);
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
                AttachContext* actx  = bs_adapter_attach_ctx_get_active();
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

    if (actx_for_window)
    {
        bs_adapter_attach_session_drain_pending_notifications(actx_for_window);
        bs_adapter_attach_session_end_write_window(actx_for_window);
    }
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
