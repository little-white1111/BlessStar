#ifndef BS_APP_SDK_CONFIG_RELOAD_SESSION_H
#define BS_APP_SDK_CONFIG_RELOAD_SESSION_H

/*
 * C-ST-7 contract block:
 * Thread safety: ConfigReloadSession is not thread-safe; all calls must be on the
 *   creating thread. An assert(thread_id_) guards each member function.
 * Error semantics: Commit() returns Report* with status FAILED on errors;
 *   AddPath/AddUri return false on null/invalid args.
 * Platform notes: Session creates a ReloadBatchController on Commit() and destroys
 *   it before returning. In-memory bytes (AddPath) are memcpy'd immediately.
 *
 * APP-PUSH-1: ConfigReloadSession session-based object
 * APP-PUSH-2: in-memory bytes memcpy'd immediately + custom read_fn dual mode
 * APP-PUSH-3: chaining + vector dual-style gates; AddPolicyGates({}) = default_gate
 * APP-PUSH-4: default_gate parse result cached, shared by subsequent gates
 * APP-PUSH-5: Create->Commit->Reset reusable; ResetGates() full replacement
 * APP-PUSH-6: Commit() synchronous blocking
 * APP-PUSH-7: Session owns Report; TakeReport() transfers ownership
 * APP-PUSH-8: Debug assert thread safety check
 * APP-PUSH-9: Destructor cleans up if Commit not called: log + reset
 *
 * Day24 UX improvements:
 * UX-P4: LastReport() const access to current (untaken) report
 * UX-P6: PathSource enum + AddMemPath/AddFilePath convenience methods
 * UX-P8: GetConfig/GetConfigByUri — key-namespace 4-B wrapper
 * UX-P10: SetAuditDir static API for mem audit log
 */

#include <cstddef>
#include <cstdint>

#include <map>
#include <string>
#include <thread>
#include <vector>

#include "bs/kernel/report/report.h"
#include "bs/kernel/common/bs_log.h"

#include "bs/adapter/attach_context.h"
#include "bs/adapter/attach_config.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/orchestration/reload_with_report.h"
#include "bs/adapter/orchestration/reload_gate_default.h"

#include "bs/app/sdk/app_scenario_policy.h"
#include "bs/app/sdk/app_session.h"
#include "bs/app/sdk/app_vendor_precheck.h"
#include "bs/app/sdk/mem_audit_log.h"

namespace bs::app
{

/**
 * PathSource enum for AddPath overloads.
 *
 *   AddPath(key, data, len)        — default (kMem), identical to old AddPath
 *   AddPath(uri, PathSource::kFile) — file URI (no data/len)
 *   AddPath(uri, PathSource::kHttp/kHttps) — returns false (not implemented)
 */
enum class PathSource
{
    kMem,     // In-memory bytes (default)
    kFile,    // File URI (delegated to IoFacade)
    kHttp,    // HTTP(S) — not implemented in MVP
    kHttps    // HTTP(S) — not implemented in MVP
};

class ConfigReloadSession
{
public:
    explicit ConfigReloadSession(AttachContext* ctx);
    ~ConfigReloadSession();

    // --- gates (R3 - chaining + vector dual style) ---
    void SetNoGate();
    void AddPolicyGate(const ScenarioPolicy& policy);
    void AddPolicyGates(const std::vector<ScenarioPolicy>& policies);
    void AddCustomGate(const CustomGateEntry& entry);
    void ResetGates();

    // --- data passing (R2 - flexible mode) ---

    /** Original AddPath: in-memory bytes (kMem). */
    bool AddPath(const char* key, const uint8_t* data, size_t len);

    /**
     * AddPath overload with PathSource enum.
     *   PathSource::kMem  — same as original AddPath (but requires data/len)
     *   PathSource::kFile — adds a file URI to pending_uris_
     *   PathSource::kHttp/kHttps — returns false (not implemented)
     * Note: This overload expects @p uri for kFile/kHttp/kHttps.
     *       For kMem, pass nullptr for uri and use AddMemPath instead.
     */
    bool AddPath(const char* uri, PathSource source);

    /** Convenience: in-memory bytes (equivalent to old AddPath). */
    bool AddMemPath(const char* key, const uint8_t* data, size_t len);

    /** Convenience: file URI (equivalent to AddUri). */
    bool AddFilePath(const char* uri);

    void SetReadFn(ReloadPathReadFn fn, void* user_ctx);
    bool AddUri(const char* uri);

    // --- execution (R5 - synchronous blocking) ---
    Report* Commit();
    Report* TakeReport();

    // --- UX-P4: Report lifecycle ---
    /** Read-only access to current (untaken) report, or nullptr. */
    const Report* LastReport() const;

    // --- UX-P8: GetConfig — key namespace 4-B wrapper ---
    /**
     * Query config from ConfigManager via ctx_.
     * If key does not have a "mem://" or other scheme prefix, it is treated as
     * a MEM key and prepended with "mem://" before lookup.
     * Returns snapshot status: 0 on success, -1 on not-found/error.
     * @param key  Config key (e.g. "myapp/config" or "mem://myapp/config")
     * @param out_data  Output pointer to snapshot data (heap-allocated, caller must free)
     * @param out_size  Output size
     */
    int GetConfig(const char* key, void** out_data, size_t* out_size) const;

    /**
     * Query config by full URI directly (no key translation).
     */
    int GetConfigByUri(const char* uri, void** out_data, size_t* out_size) const;

    // --- lifecycle (R4 - reusable) ---
    void Reset();

    // --- UX-P10: Audit directory (static) ---
    /** Set global audit directory for mem audit logs. Thread-safe one-shot. */
    static void SetAuditDir(const char* dir);

private:
    ReloadBatchController*                     ctrl_             = nullptr;
    AttachContext*                             ctx_              = nullptr;
    Report*                                    report_           = nullptr;
    bool                                       report_taken_     = false;
    bool                                       no_gate_          = false;
    std::thread::id                            thread_id_;
    std::vector<ScenarioPolicy>                policy_gates_;
    std::vector<CustomGateEntry>               custom_gates_;
    std::map<std::string, std::vector<uint8_t>> pending_bytes_;
    std::vector<std::string>                   pending_uris_;
    ReloadPathReadFn                           user_read_fn_     = nullptr;
    void*                                      user_read_fn_ctx_ = nullptr;

    // --- private helpers ---
    /** Write mem audit log for a pending_bytes_ entry. */
    void write_audit_log(const char* key, const void* data, size_t len);
};

} // namespace bs::app

#endif // BS_APP_SDK_CONFIG_RELOAD_SESSION_H
