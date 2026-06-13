#include "bs/app/sdk/config_reload_session.h"

#include "bs/adapter/parser/config_parse.h"
#include "bs/adapter/attach_context.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <new>
#include <string>
#include <utility>

namespace bs::app
{

// ---------------------------------------------------------------------------
// Internal read context for in-memory + user read_fn dispatching
// ---------------------------------------------------------------------------
struct SessionCommitContext
{
    const std::map<std::string, std::vector<uint8_t>>* pending_bytes;
    ReloadPathReadFn                                   user_read_fn;
    void*                                              user_read_fn_ctx;
    const std::vector<ScenarioPolicy>*                 policy_gates;
    const std::vector<CustomGateEntry>*                custom_gates;
    bool                                               no_gate;
};

// ---------------------------------------------------------------------------
// In-memory + user-delegating read function (ReloadPathReadFn)
// ---------------------------------------------------------------------------
static int session_read_fn(void* user_ctx, const char* uri, IoReadResult* out)
{
    auto* ctx = static_cast<SessionCommitContext*>(user_ctx);
    bs_io_read_result_init(out);
    if (!uri || !out)
        return BS_IO_ERR_INVALID_ARG;

    // "mem://" URIs read from pending_bytes_
    if (std::strncmp(uri, "mem://", 6) == 0)
    {
        const char* key = uri + 6;
        auto        it  = ctx->pending_bytes->find(key);
        if (it == ctx->pending_bytes->end())
        {
            out->status = BS_IO_ERR_NOT_FOUND;
            return BS_IO_ERR_NOT_FOUND;
        }
        out->data = static_cast<uint8_t*>(std::malloc(it->second.size()));
        if (!out->data)
        {
            out->status = BS_IO_ERR_PROVIDER;
            return BS_IO_ERR_PROVIDER;
        }
        std::memcpy(out->data, it->second.data(), it->second.size());
        out->length     = it->second.size();
        out->status     = 0;
        out->source_uri = strdup(uri);
        return 0;
    }

    // Fall back to user-provided read_fn
    if (ctx->user_read_fn)
        return ctx->user_read_fn(ctx->user_read_fn_ctx, uri, out);

    return BS_IO_ERR_UNSUPPORTED_SCHEME;
}

// ---------------------------------------------------------------------------
// Composite gate chain: default_gate -> policy_gates -> custom_gates
// ---------------------------------------------------------------------------
static int session_gate_fn(void* user_ctx, const char* uri,
                           const IoReadResult* read_result,
                           BsReloadGateDetail* detail_out)
{
    auto* ctx = static_cast<SessionCommitContext*>(user_ctx);

    // SetNoGate -> skip everything
    if (ctx->no_gate)
        return BS_RELOAD_GATE_OK;

    if (!read_result || !read_result->data || read_result->length == 0)
    {
        if (detail_out)
            std::snprintf(detail_out->buf, sizeof(detail_out->buf), "empty read result");
        return BS_RELOAD_GATE_PARSE_FAIL;
    }

    // ---- Step 1: default_gate (format parse + verify) ----
    BsConfigParseResult parse_result{};
    int rc = bs_adapter_attach_reload_parse_and_verify_bytes(read_result, &parse_result, detail_out);
    if (rc != BS_RELOAD_GATE_OK)
    {
        bs_adapter_parser_result_destroy(&parse_result);
        return rc;
    }
    // parse_result is available as "cache" for subsequent gates (APP-PUSH-4)

    // ---- Step 2: policy gates ----
    for (const auto& policy : *ctx->policy_gates)
    {
        std::string err;
        if (!PrecheckV1InstructionsForScenario(read_result->data, read_result->length,
                                                parse_result.instructions, policy, &err))
        {
            if (detail_out)
                std::snprintf(detail_out->buf, sizeof(detail_out->buf), "%s", err.c_str());
            bs_adapter_parser_result_destroy(&parse_result);
            return BS_RELOAD_GATE_IR_REJECT;
        }
    }

    // ---- Step 3: custom gates ----
    for (const auto& entry : *ctx->custom_gates)
    {
        char err_buf[256] = {};
        if (entry.fn(read_result->data, read_result->length, err_buf, sizeof(err_buf), entry.user_ctx) != 0)
        {
            if (detail_out)
                std::snprintf(detail_out->buf, sizeof(detail_out->buf), "%s", err_buf);
            bs_adapter_parser_result_destroy(&parse_result);
            return BS_RELOAD_GATE_IR_REJECT;
        }
    }

    bs_adapter_parser_result_destroy(&parse_result);
    return BS_RELOAD_GATE_OK;
}

// ---------------------------------------------------------------------------
// Global audit directory (UX-P10)
// ---------------------------------------------------------------------------
static char g_audit_dir[1024] = {};

/* static */ void ConfigReloadSession::SetAuditDir(const char* dir)
{
    if (!dir || dir[0] == '\0')
        return;
#ifdef _MSC_VER
    strncpy_s(g_audit_dir, dir, sizeof(g_audit_dir) - 1);
#else
    std::strncpy(g_audit_dir, dir, sizeof(g_audit_dir) - 1);
#endif
    g_audit_dir[sizeof(g_audit_dir) - 1] = '\0';
}

void ConfigReloadSession::write_audit_log(const char* key, const void* data, size_t len)
{
    // Determine audit directory: SetAuditDir > BS_AUDIT_DIR env var > disabled
    const char* dir = g_audit_dir[0] != '\0' ? g_audit_dir : nullptr;
    if (!dir)
        dir = std::getenv("BS_AUDIT_DIR");
    if (!dir || dir[0] == '\0')
        return; // Audit disabled — silent degrade

    static MemAuditLog s_audit_log;
    static bool s_audit_initialized = false;
    if (!s_audit_initialized)
    {
        s_audit_initialized = s_audit_log.Init(dir);
        if (!s_audit_initialized)
        {
            BsLogState* log = ctx_ ? bs_adapter_attach_ctx_log_state(ctx_) : nullptr;
            if (log)
            {
                bs_log_emit_ctx(log, 0, BS_LOG_WARN,
                                "MemAuditLog init failed (dir=%s): %s",
                                dir, s_audit_log.GetLastError());
            }
        }
    }

    if (s_audit_initialized)
    {
        s_audit_log.Record(key, data, len);
    }
}

// ---------------------------------------------------------------------------
// ConfigReloadSession implementation
// ---------------------------------------------------------------------------

ConfigReloadSession::ConfigReloadSession(AttachContext* ctx)
    : ctx_(ctx)
    , thread_id_(std::this_thread::get_id())
{
}

ConfigReloadSession::~ConfigReloadSession()
{
    // APP-PUSH-9: destructor cleans up un-Committed resources
    if (ctrl_)
    {
        bs_adapter_attach_reload_batch_destroy(ctrl_);
        ctrl_ = nullptr;
    }
    if (report_ && !report_taken_)
    {
        bs_report_destroy(report_);
        report_ = nullptr;
    }
    if (ctx_)
    {
        BsLogState* log = bs_adapter_attach_ctx_log_state(ctx_);
        if (log)
        {
            bs_log_emit_ctx(log, 0, BS_LOG_INFO,
                            "ConfigReloadSession destroyed: report=%s taken=%d",
                            report_ ? "set" : "null", static_cast<int>(report_taken_));
        }
    }
}

// -- Gate configuration ---------------------------------------------------

void ConfigReloadSession::SetNoGate()
{
    assert(thread_id_ == std::this_thread::get_id());
    no_gate_ = true;
}

void ConfigReloadSession::AddPolicyGate(const ScenarioPolicy& policy)
{
    assert(thread_id_ == std::this_thread::get_id());
    policy_gates_.push_back(policy);
}

void ConfigReloadSession::AddPolicyGates(const std::vector<ScenarioPolicy>& policies)
{
    assert(thread_id_ == std::this_thread::get_id());
    policy_gates_.insert(policy_gates_.end(), policies.begin(), policies.end());
}

void ConfigReloadSession::AddCustomGate(
    int (*fn)(const void* data, size_t len, char* err, size_t err_cap, void* ctx),
    void* user_ctx)
{
    assert(thread_id_ == std::this_thread::get_id());
    if (!fn)
        return;
    custom_gates_.push_back({fn, user_ctx});
}

void ConfigReloadSession::ResetGates()
{
    assert(thread_id_ == std::this_thread::get_id());
    no_gate_ = false;
    policy_gates_.clear();
    custom_gates_.clear();
}

// -- Data passing ----------------------------------------------------------

bool ConfigReloadSession::AddPath(const char* key, const uint8_t* data, size_t len)
{
    assert(thread_id_ == std::this_thread::get_id());
    if (!key || !data || len == 0)
        return false;

    // APP-PUSH-2: immediate memcpy
    try
    {
        std::vector<uint8_t> copy(data, data + len);
        pending_bytes_[key] = std::move(copy);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool ConfigReloadSession::AddPath(const char* uri, PathSource source)
{
    assert(thread_id_ == std::this_thread::get_id());

    if (!uri || uri[0] == '\0')
        return false;

    switch (source)
    {
    case PathSource::kFile:
        // File URI -> add to pending_uris_
        try
        {
            pending_uris_.push_back(uri);
        }
        catch (...)
        {
            return false;
        }
        return true;

    case PathSource::kHttp:
    case PathSource::kHttps:
        // Not implemented in MVP
        return false;

    case PathSource::kMem:
    default:
        // For kMem, use AddPath(key, data, len) or AddMemPath instead.
        return false;
    }
}

bool ConfigReloadSession::AddMemPath(const char* key, const uint8_t* data, size_t len)
{
    return AddPath(key, data, len);
}

bool ConfigReloadSession::AddFilePath(const char* uri)
{
    return AddUri(uri);
}

void ConfigReloadSession::SetReadFn(ReloadPathReadFn fn, void* user_ctx)
{
    assert(thread_id_ == std::this_thread::get_id());
    user_read_fn_     = fn;
    user_read_fn_ctx_ = user_ctx;
}

bool ConfigReloadSession::AddUri(const char* uri)
{
    assert(thread_id_ == std::this_thread::get_id());
    if (!uri || uri[0] == '\0')
        return false;
    try
    {
        pending_uris_.push_back(uri);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

// -- Execution (UX-P7: two-phase commit with batch controller) ------------

Report* ConfigReloadSession::Commit()
{
    assert(thread_id_ == std::this_thread::get_id());

    // Clean up previous report if not taken
    if (report_ && !report_taken_)
    {
        bs_report_destroy(report_);
        report_ = nullptr;
    }

    report_ = bs_report_create("config_reload_session");
    report_taken_ = false;
    bs_report_mark_start(report_);

    if (!ctx_)
    {
        bs_report_add_error(report_, "session", "null attach context");
        bs_report_set_status(report_, REPORT_STATUS_FAILED);
        bs_report_mark_end(report_);
        return report_;
    }

    if (pending_bytes_.empty() && pending_uris_.empty())
    {
        bs_report_add_error(report_, "session", "no paths added");
        bs_report_set_status(report_, REPORT_STATUS_FAILED);
        bs_report_mark_end(report_);
        return report_;
    }

    // Build commit context for read_fn + gate_fn callbacks
    SessionCommitContext commit_ctx;
    commit_ctx.pending_bytes     = &pending_bytes_;
    commit_ctx.user_read_fn      = user_read_fn_;
    commit_ctx.user_read_fn_ctx  = user_read_fn_ctx_;
    commit_ctx.policy_gates      = &policy_gates_;
    commit_ctx.custom_gates      = &custom_gates_;
    commit_ctx.no_gate           = no_gate_;

    // =====================================================================
    // Phase 1: FILE URIs (pending_uris_)
    // Use ReloadBatchController for proper pipeline/state transitions.
    // =====================================================================
    if (!pending_uris_.empty())
    {
        ctrl_ = bs_adapter_attach_reload_batch_create(8);
        if (!ctrl_)
        {
            bs_report_add_error(report_, "session", "phase1: failed to create batch controller");
            bs_report_set_status(report_, REPORT_STATUS_FAILED);
            bs_report_mark_end(report_);
            return report_;
        }

        // Bind attach context and scheme
        bs_adapter_attach_reload_batch_set_attach_ctx(ctrl_, ctx_);
        bs_adapter_attach_reload_batch_set_attach_scheme(ctrl_, BS_ATTACH_SCHEME_PER_PATH);

        // Set read_fn and gate_fn
        if (user_read_fn_)
        {
            bs_adapter_attach_reload_batch_set_read_fn(ctrl_, session_read_fn, &commit_ctx);
        }
        bs_adapter_attach_reload_batch_set_gate_fn(ctrl_, session_gate_fn, &commit_ctx);

        // Add FILE URIs
        for (const auto& uri : pending_uris_)
        {
            if (bs_adapter_attach_reload_batch_add_path(ctrl_, uri.c_str()) != 0)
            {
                char buf[256];
                std::snprintf(buf, sizeof(buf), "phase1: failed to add uri: %s", uri.c_str());
                bs_report_add_error(report_, "session", buf);
            }
        }

        // Run phase 1 (FILE)
        bs_adapter_attach_reload_batch_set_report(ctrl_, report_);
        int rc = bs_adapter_attach_reload_batch_run(ctrl_);

        // UX-P2: check batch outcome (rc may be 0 even if individual paths failed)
        BatchOutcome outcome = bs_adapter_attach_reload_batch_outcome(ctrl_);
        bool phase1_ok = (rc == 0 && outcome == BATCH_ALL_OK);

        if (!phase1_ok)
        {
            bs_report_set_status(report_, REPORT_STATUS_FAILED);
        }

        // Clean up controller (before checking outcome, so it's destroyed before Phase 2)
        bs_adapter_attach_reload_batch_destroy(ctrl_);
        ctrl_ = nullptr;

        // If Phase 1 had failures, do NOT proceed to Phase 2 (MEM)
        if (!phase1_ok)
        {
            bs_report_mark_end(report_);
            return report_;
        }
    }

    // =====================================================================
    // Phase 2: MEM paths (pending_bytes_)
    // Gate + sync directly (no persist — in-memory only).
    // Only executed if Phase 1 succeeded (or there were no FILE paths).
    // =====================================================================
    if (!pending_bytes_.empty())
    {
        for (const auto& kv : pending_bytes_)
        {
            std::string mem_uri = std::string("mem://") + kv.first;

            // Build a fake IoReadResult for the gate
            uint8_t* data_copy = static_cast<uint8_t*>(std::malloc(kv.second.size()));
            if (!data_copy)
            {
                bs_report_add_error(report_, "session", "phase2: OOM");
                bs_report_set_status(report_, REPORT_STATUS_FAILED);
                bs_report_mark_end(report_);
                return report_;
            }
            std::memcpy(data_copy, kv.second.data(), kv.second.size());

            IoReadResult fake_result;
            bs_io_read_result_init(&fake_result);
            fake_result.data       = data_copy;
            fake_result.length     = kv.second.size();
            fake_result.status     = 0;
            fake_result.source_uri = strdup(mem_uri.c_str());

            // Run composite gate
            {
                BsReloadGateDetail gate_detail{};
                int gate_rc = session_gate_fn(&commit_ctx, mem_uri.c_str(), &fake_result, &gate_detail);
                if (gate_rc != BS_RELOAD_GATE_OK)
                {
                    char buf[256];
                    std::snprintf(buf, sizeof(buf),
                                  "phase2: gate rejected mem key=%s (rc=%d): %s",
                                  kv.first.c_str(), gate_rc, gate_detail.buf);
                    bs_report_add_error(report_, "session", buf);
                    bs_report_set_status(report_, REPORT_STATUS_FAILED);
                    bs_io_read_result_free(&fake_result);
                    bs_report_mark_end(report_);
                    return report_;
                }
            }

            // MD-D-03: Re-parse to get IRInstructionList, inject into gate_cache
            {
                BsConfigParseResult parse_result{};
                BsStatus pst = bs_adapter_parser_parse_bytes(kv.second.data(), kv.second.size(),
                                                              &parse_result);
                if (bs_status_is_ok(pst) && parse_result.instructions)
                {
                    bs_adapter_attach_ctx_set_gate_result(ctx_, mem_uri.c_str(),
                                                          parse_result.instructions);
                }
                // parse_result.instructions ownership: set_gate_result deep-copies,
                // parser_result_destroy frees the original
                bs_adapter_parser_result_destroy(&parse_result);
            }

            bs_io_read_result_free(&fake_result);

            // Sync directly to ConfigManager (no persist)
            int sync_rc = bs_adapter_attach_config_sync_path(
                ctx_, mem_uri.c_str(), kv.second.data(), kv.second.size());
            if (sync_rc != 0)
            {
                char buf[256];
                std::snprintf(buf, sizeof(buf),
                              "phase2: sync_path failed mem key=%s (rc=%d)",
                              kv.first.c_str(), sync_rc);
                bs_report_add_error(report_, "session", buf);
                bs_report_set_status(report_, REPORT_STATUS_FAILED);
                bs_report_mark_end(report_);
                return report_;
            }

            // Write audit log (silently degrades if audit dir not configured)
            write_audit_log(kv.first.c_str(), kv.second.data(), kv.second.size());
        }
    }

    // If status is still the initial SUCCESS (default from report_create), that's correct
    // If any phase set it to FAILED, it stays FAILED.
    // If neither phase ran, set FAILED.
    if (pending_bytes_.empty() && pending_uris_.empty())
    {
        // Intentionally empty — already handled above
    }

    bs_report_mark_end(report_);
    return report_;
}

Report* ConfigReloadSession::TakeReport()
{
    assert(thread_id_ == std::this_thread::get_id());
    if (report_taken_)
        return nullptr;
    Report* r       = report_;
    report_         = nullptr;
    report_taken_   = true;
    return r;
}

const Report* ConfigReloadSession::LastReport() const
{
    assert(thread_id_ == std::this_thread::get_id());
    if (report_taken_)
        return nullptr;
    return report_;
}

int ConfigReloadSession::GetConfig(const char* key, void** out_data, size_t* out_size) const
{
    assert(thread_id_ == std::this_thread::get_id());
    if (!key || !out_data || !out_size || !ctx_)
        return -1;

    // Key namespace 4-B: if key has no scheme prefix, treat as MEM key
    std::string lookup_uri;
    if (std::strchr(key, ':') == nullptr)
    {
        // No scheme — MEM key, prepend mem://
        lookup_uri = std::string("mem://") + key;
    }
    else
    {
        lookup_uri = key;
    }

    return GetConfigByUri(lookup_uri.c_str(), out_data, out_size);
}

int ConfigReloadSession::GetConfigByUri(const char* uri, void** out_data, size_t* out_size) const
{
    assert(thread_id_ == std::this_thread::get_id());
    if (!uri || !out_data || !out_size || !ctx_)
        return -1;

    // Use the adapter-level config API (already linked via bs_adapter_attach)
    return bs_adapter_attach_config_get_snapshot(ctx_, uri, out_data, out_size);
}

// -- Lifecycle -------------------------------------------------------------

void ConfigReloadSession::Reset()
{
    assert(thread_id_ == std::this_thread::get_id());

    if (ctrl_)
    {
        bs_adapter_attach_reload_batch_destroy(ctrl_);
        ctrl_ = nullptr;
    }
    if (report_ && !report_taken_)
    {
        bs_report_destroy(report_);
        report_ = nullptr;
    }
    report_taken_ = false;

    // APP-PUSH-5: keep gate config, clear data paths
    pending_bytes_.clear();
    pending_uris_.clear();
}

} // namespace bs::app
