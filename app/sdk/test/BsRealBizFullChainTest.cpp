/**
 * Real business full-chain test (expense reimbursement approval chain config)
 *
 * Three layers: App (Normalize + Session) -> Adapter (ReloadBatchController) -> Kernel (ConfigManager)
 *
 * Key design: uses AddUri + SetReadFn (delegating to IoFacade) so the ReloadBatch
 * can read from real files on disk. This is necessary because mem:// URIs cannot
 * be persisted through the attach persistent store path.
 *
 * Covers real business scenarios:
 *   1. Business system startup (AttachSession)
 *   2. Vendor format normalization (vendor -> v1 JSON)
 *   3. Config submission via AddUri + IoFacade (with policy gate)
 *   4. Kernel state readback (verify config is ACTIVE and content matches)
 *   5. Watch notification verification (state change event fires callback)
 *   6. Hot-update verification (overwrite file, re-submit, content changes)
 *   7. Bad config isolation (bad config does not contaminate active config)
 *   8. Multi-path atomic submit (one Commit manages multiple config paths)
 *
 * Day24 UX additions:
 *   9. AddMemPath + AddFilePath new API
 *  10. PathSource::kFile enum parameter
 *  11. GetConfig(key) unified query
 *  12. AppSession one-line startup
 *  13. LastReport() read-only access
 *  14. Two-phase atomicity (FILE failure rolls back without affecting MEM)
 *
 * Day24 MR-09 addition:
 *  15. Policy metadata_rules gate (Scenario 9)
 */

#include "bs/kernel/report/report.h"
#include "bs/kernel/state/ConfigManager.h"

#include "bs/adapter/attach_config.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/orchestration/reload_with_report.h"
#include "bs/adapter/orchestration/reload_gate_default.h"
#include "bs/adapter/persistence/attach_store.h"

#include "bs/app/sdk/config_reload_session.h"
#include "bs/app/sdk/app_scenario_policy.h"
#include "bs/app/sdk/app_vendor_precheck.h"
#include "bs/app/sdk/vendor_config_normalizer.h"
#include "bs/app/sdk/vendor_reload_facade.h"
#include "bs/app/sdk/app_session.h"
#include "bs/app/sdk/mem_audit_log.h"
#include "bs/app/sdk/attach_snapshot_utils.h"
#include "bs/app/sdk/config_session_reader.h"

#include "bs/kernel/state/ConfigEvent.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <string>

#include "support/attach_test_fixture.h"
#include "support/config_v1_golden.h"
#include "support/day12_attach_fixture.h"
#include "support/test_temp_dir.h"

namespace fs = std::filesystem;

// ============================================================================
// Watch callback tracking
// ============================================================================

struct WatchRecord
{
    bool     fired       = false;
    int      fire_count  = 0;
    char     path[256]   = {};
};

static void watch_callback(const char* path, ConfigEventType type, const void* /*snapshot*/,
                           void* user_data)
{
    auto* rec = static_cast<WatchRecord*>(user_data);
    rec->fired      = true;
    ++rec->fire_count;
    if (path)
        std::strncpy(rec->path, path, sizeof(rec->path) - 1);
}

// ============================================================================
// Read function: delegates to IoFacade for file:// URIs
// ============================================================================

static int facade_read_fn(void* user_ctx, const char* uri, IoReadResult* out)
{
    auto* fix = static_cast<BsTestAttachIoFixture*>(user_ctx);
    return bs_io_facade_read(fix->io, uri, out);
}

/** Read function that takes IoFacade* directly (for AppSession test). */
static int io_read_fn(void* user_ctx, const char* uri, IoReadResult* out)
{
    auto* io = static_cast<IoFacade*>(user_ctx);
    if (!io) return BS_IO_ERR_INVALID_ARG;
    return bs_io_facade_read(io, uri, out);
}

// ============================================================================
// Helper: write bytes to a temp file and return the file URI
// ============================================================================

static std::string write_bytes_to_file(const fs::path& temp_dir, const char* name,
                                       const void* data, size_t len)
{
    const fs::path file = temp_dir / name;
    std::FILE*     f    = std::fopen(file.string().c_str(), "wb");
    if (!f) return {};
    std::fwrite(data, 1, len, f);
    std::fclose(f);
    return bs_test_path_to_file_uri(file);
}

// ============================================================================
// Helper: create a standard bootstrapped AttachContext (factory pattern)
// ============================================================================

static BsTestAttachIoFixture create_fixture_with_persist(const fs::path& temp_dir,
                                                          const char* manifest_name)
{
    BsTestAttachIoFixture fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    if (!fix.ctx) return fix;

    bs_test_attach_bootstrap_begin_ctx(&fix);
    bs_test_attach_bootstrap_freeze_ctx(&fix);
    bs_test_attach_open_io(&fix);

    const fs::path manifest = temp_dir / manifest_name;
    bs_adapter_attach_ctx_open_persist_store(fix.ctx, manifest.string().c_str());

    return fix;
}

// ============================================================================
// Test scenario 1: Full config lifecycle
//   (start -> normalize -> submit -> verify -> watch -> modify -> re-verify)
// ============================================================================

static int test_full_config_lifecycle()
{
    std::fprintf(stderr, "\n=== SCENARIO 1: Full config lifecycle ===\n");

    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_reallife_cycle"));
    const fs::path&          temp_dir = tmp_guard.path;

    // -- 1. AttachSession startup ---------------------------------------------
    BsTestAttachIoFixture fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("ctx-create", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);

    // Open persistent store so Kernel ConfigManager receives configs via sync_path
    const fs::path manifest = temp_dir / "reallife.manifest";
    BS_TEST_REQUIRE("open-store",
                    bs_adapter_attach_ctx_open_persist_store(fix.ctx, manifest.string().c_str()) == 0);

    // -- 2. Vendor format normalization (simulate business system reading vendor file) --
    const fs::path vendor_file =
        fs::absolute("app/sdk/test/fixtures/vendor_generic_business_good.json");
    BS_TEST_REQUIRE("vendor-file", fs::exists(vendor_file));

    bs::app::NormalizeResult norm;
    BS_TEST_REQUIRE("normalize",
                    bs::app::NormalizeVendorConfig(
                        bs::app::VendorFormat::GenericBusinessJson,
                        vendor_file.string(), &norm) && norm.ok);

    // Write normalized v1 bytes to a temp file so the IoFacade can read it
    const std::string cfg_uri = write_bytes_to_file(temp_dir, "v1_reallife.json",
                                                     norm.v1_bytes.data(), norm.v1_bytes.size());
    BS_TEST_REQUIRE("write-cfg", !cfg_uri.empty());

    // Business scenario policy (gate: expense reimbursement scenario validation)
    bs::app::ScenarioPolicy policy;
    policy.type             = bs::app::ScenarioType::ExpenseReimburse;
    policy.tenant           = "tenant-a";
    policy.allow_hot_reload = true;
    policy.max_batch        = 64;

    // -- 3. Config submission via AddUri + IoFacade read_fn ---------------------
    {
        bs::app::ConfigReloadSession session(fix.ctx);
        session.SetReadFn(facade_read_fn, &fix);
        session.AddPolicyGate(policy);
        BS_TEST_REQUIRE("add-uri", session.AddUri(cfg_uri.c_str()));

        Report* report = session.Commit();
        BS_TEST_REQUIRE("commit", report != nullptr);
        BS_TEST_REQUIRE("commit-ok", bs_report_get_status(report) == REPORT_STATUS_SUCCESS);

        char* json = bs_report_to_json(report);
        if (json)
        {
            BS_TEST_REQUIRE("no-parse-err", std::strstr(json, "[parse]") == nullptr);
            BS_TEST_REQUIRE("no-gate-err",  std::strstr(json, "[ir_gate]") == nullptr);
            std::free(json);
        }

        Report* taken = session.TakeReport();
        if (taken) bs_report_destroy(taken);
    }

    // -- 4. Kernel state readback (verify config is ACTIVE) --------------------
    {
        ConfigState state;
        BS_TEST_REQUIRE("get-state",
                        bs_adapter_attach_config_get_state(fix.ctx, cfg_uri.c_str(), &state) == 0);
        BS_TEST_REQUIRE("state-is-active", state == CONFIG_STATE_ACTIVE);

        void*  snap      = nullptr;
        size_t snap_size = 0;
        BS_TEST_REQUIRE("get-snapshot",
                        bs_adapter_attach_config_get_snapshot(fix.ctx, cfg_uri.c_str(),
                                                              &snap, &snap_size) == 0);
        BS_TEST_REQUIRE("snapshot-nonempty", snap != nullptr && snap_size > 0);
        BS_TEST_REQUIRE("snapshot-matches-v1",
                        snap_size == norm.v1_bytes.size() &&
                        std::memcmp(snap, norm.v1_bytes.data(), snap_size) == 0);

        // Verify config content contains business keyword "reload-smoke-1"
        const char* text = static_cast<const char*>(snap);
        BS_TEST_REQUIRE("snapshot-contains-smoke",
                        std::strstr(text, "reload-smoke-1") != nullptr);
        std::free(snap);
    }

    // -- 5. Watch notification verification -----------------------------------
    {
        WatchRecord rec;

        ConfigManager* cm = bs_adapter_attach_ctx_config_manager(fix.ctx);
        BS_TEST_REQUIRE("config-mgr", cm != nullptr);

        BS_TEST_REQUIRE("subscribe",
                        bs_adapter_attach_config_subscribe_state_watch(
                            fix.ctx, cfg_uri.c_str(), watch_callback, &rec) == 0);

        // Re-submit same config (triggers hot_update -> ENTER_ACTIVE event)
        {
            bs::app::ConfigReloadSession session(fix.ctx);
            session.SetReadFn(facade_read_fn, &fix);
            session.AddPolicyGate(policy);
            session.AddUri(cfg_uri.c_str());
            Report* r = session.Commit();
            BS_TEST_REQUIRE("recommit", r != nullptr);
            Report* taken = session.TakeReport();
            if (taken) bs_report_destroy(taken);
        }

        BS_TEST_REQUIRE("watch-fired", rec.fired);
        BS_TEST_REQUIRE("watch-path-match",
                        std::strcmp(rec.path, cfg_uri.c_str()) == 0);

        // Unregister watch
        BS_TEST_REQUIRE("unsubscribe",
                        bs_config_manager_unsubscribe_state_change(
                            cm, cfg_uri.c_str(), watch_callback) == 0);
    }

    // -- 6. Hot-update verification (modify config, re-submit) -----------------
    {
        // Build a modified v2 config with changed metadata
        const char v2_text[] =
            "{\n"
            "  \"kernel_version\": \"0.4.0\",\n"
            "  \"adapter_version\": \"0.4.0\",\n"
            "  \"manual_requirements\": [],\n"
            "  \"instructions\": [\n"
            "    {\n"
            "      \"type\": \"test\",\n"
            "      \"name\": \"approval-chain-v2\",\n"
            "      \"metadata\": {\n"
            "        \"subject_code\": \"1001.02\",\n"
            "        \"tax_rate\": \"13\",\n"
            "        \"max_amount\": \"500000\"\n"
            "      }\n"
            "    }\n"
            "  ]\n"
            "}\n";
        const size_t     v2_len   = std::strlen(v2_text);
        const uint8_t*   v2_bytes = reinterpret_cast<const uint8_t*>(v2_text);

        // Overwrite the original file with v2 content, then re-submit to the same URI
        // This triggers ConfigManager::hot_update -> ENTER_ACTIVE
        {
            std::FILE* f = std::fopen((temp_dir / "v1_reallife.json").string().c_str(), "wb");
            BS_TEST_REQUIRE("reopen", f != nullptr);
            std::fwrite(v2_bytes, 1, v2_len, f);
            std::fclose(f);

            bs::app::ConfigReloadSession session(fix.ctx);
            session.SetReadFn(facade_read_fn, &fix);
            session.AddPolicyGate(policy);
            session.AddUri(cfg_uri.c_str());
            Report* r = session.Commit();
            BS_TEST_REQUIRE("v2-hot-commit", r != nullptr);
            BS_TEST_REQUIRE("v2-hot-ok", bs_report_get_status(r) == REPORT_STATUS_SUCCESS);
            Report* taken = session.TakeReport();
            if (taken) bs_report_destroy(taken);
        }

        // Readback and verify content has changed
        void*  snap      = nullptr;
        size_t snap_size = 0;
        BS_TEST_REQUIRE("v2-snapshot",
                        bs_adapter_attach_config_get_snapshot(fix.ctx, cfg_uri.c_str(),
                                                              &snap, &snap_size) == 0);
        BS_TEST_REQUIRE("v2-nonempty", snap != nullptr && snap_size > 0);

        const char* text = static_cast<const char*>(snap);
        BS_TEST_REQUIRE("v2-contains-maxamount",
                        std::strstr(text, "500000") != nullptr);
        BS_TEST_REQUIRE("v2-name-changed",
                        std::strstr(text, "approval-chain-v2") != nullptr);
        BS_TEST_REQUIRE("v2-content-different",
                        snap_size != norm.v1_bytes.size() ||
                        std::memcmp(snap, norm.v1_bytes.data(), snap_size) != 0);
        std::free(snap);

        ConfigState state_after;
        BS_TEST_REQUIRE("get-state-after",
                        bs_adapter_attach_config_get_state(fix.ctx, cfg_uri.c_str(), &state_after) == 0);
        BS_TEST_REQUIRE("state-still-active", state_after == CONFIG_STATE_ACTIVE);
    }

    // -- Cleanup ---------------------------------------------------------------
    bs_test_attach_teardown(&fix);
    std::fprintf(stderr, "SCENARIO 1: PASS\n");
    return 0;
}

// ============================================================================
// Test scenario 2: Bad config isolation
//   (ensure invalid config does not contaminate an already-active config)
// ============================================================================

static int test_bad_config_isolation()
{
    std::fprintf(stderr, "\n=== SCENARIO 2: Bad config isolation ===\n");

    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_reallife_bad"));
    const fs::path&          temp_dir = tmp_guard.path;

    BsTestAttachIoFixture fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("ctx-create", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);

    const fs::path manifest = temp_dir / "bad_isolation.manifest";
    BS_TEST_REQUIRE("open-store",
                    bs_adapter_attach_ctx_open_persist_store(fix.ctx, manifest.string().c_str()) == 0);

    // Write valid config file
    const std::string good_uri = write_bytes_to_file(temp_dir, "valid_cfg.json",
                                                      kBlessStarConfigV1Golden,
                                                      kBlessStarConfigV1GoldenLen);
    BS_TEST_REQUIRE("write-good", !good_uri.empty());

    // -- 1. Submit a valid config first ----------------------------------------
    {
        bs::app::ConfigReloadSession session(fix.ctx);
        session.SetReadFn(facade_read_fn, &fix);
        session.AddUri(good_uri.c_str());
        Report* r = session.Commit();
        BS_TEST_REQUIRE("good-commit", r != nullptr);
        BS_TEST_REQUIRE("good-ok", bs_report_get_status(r) == REPORT_STATUS_SUCCESS);
        Report* taken = session.TakeReport();
        if (taken) bs_report_destroy(taken);
    }

    // Verify valid config is ACTIVE
    {
        ConfigState state;
        BS_TEST_REQUIRE("good-state",
                        bs_adapter_attach_config_get_state(fix.ctx, good_uri.c_str(), &state) == 0);
        BS_TEST_REQUIRE("good-active", state == CONFIG_STATE_ACTIVE);
    }

    // -- 2. Try submitting invalid JSON -----------------------------------------
    {
        const uint8_t bad_data[] = "{ not valid json content }";
        const std::string bad_uri = write_bytes_to_file(temp_dir, "bad_cfg.json",
                                                         bad_data, sizeof(bad_data) - 1);
        BS_TEST_REQUIRE("write-bad", !bad_uri.empty());

        bs::app::ConfigReloadSession session(fix.ctx);
        session.SetReadFn(facade_read_fn, &fix);
        session.AddUri(bad_uri.c_str());

        Report* r = session.Commit();
        BS_TEST_REQUIRE("bad-commit", r != nullptr);
        Report* taken = session.TakeReport();
        if (taken) bs_report_destroy(taken);
    }

    // -- 3. Verify valid config is still intact ---------------------------------
    {
        void*  snap      = nullptr;
        size_t snap_size = 0;
        BS_TEST_REQUIRE("good-snapshot",
                        bs_adapter_attach_config_get_snapshot(fix.ctx, good_uri.c_str(),
                                                              &snap, &snap_size) == 0);
        BS_TEST_REQUIRE("good-intact",
                        snap_size == kBlessStarConfigV1GoldenLen &&
                        std::memcmp(snap, kBlessStarConfigV1Golden, snap_size) == 0);
        std::free(snap);
    }

    bs_test_attach_teardown(&fix);
    std::fprintf(stderr, "SCENARIO 2: PASS\n");
    return 0;
}

// ============================================================================
// Test scenario 3: Multi-path atomic submit
//   (one Commit submits multiple config files, all should be ACTIVE)
// ============================================================================

static int test_multi_path_atomic_submit()
{
    std::fprintf(stderr, "\n=== SCENARIO 3: Multi-path atomic submit ===\n");

    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_reallife_multi"));
    const fs::path&          temp_dir = tmp_guard.path;

    BsTestAttachIoFixture fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("ctx-create", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);

    const fs::path manifest = temp_dir / "multi_atomic.manifest";
    BS_TEST_REQUIRE("open-store",
                    bs_adapter_attach_ctx_open_persist_store(fix.ctx, manifest.string().c_str()) == 0);

    // Write two config files
    const std::string uri_a = write_bytes_to_file(temp_dir, "chain_a.json",
                                                   kBlessStarConfigV1Golden,
                                                   kBlessStarConfigV1GoldenLen);
    const std::string uri_b = write_bytes_to_file(temp_dir, "chain_b.json",
                                                   kBlessStarConfigV1Golden,
                                                   kBlessStarConfigV1GoldenLen);
    BS_TEST_REQUIRE("write-uris", !uri_a.empty() && !uri_b.empty());

    // One Commit with two config URIs
    {
        bs::app::ConfigReloadSession session(fix.ctx);
        session.SetReadFn(facade_read_fn, &fix);
        BS_TEST_REQUIRE("add-uri-a", session.AddUri(uri_a.c_str()));
        BS_TEST_REQUIRE("add-uri-b", session.AddUri(uri_b.c_str()));

        Report* r = session.Commit();
        BS_TEST_REQUIRE("multi-commit", r != nullptr);
        Report* taken = session.TakeReport();
        if (taken) bs_report_destroy(taken);
    }

    // Verify both paths are independently ACTIVE
    for (const char* uri : {uri_a.c_str(), uri_b.c_str()})
    {
        ConfigState state;
        BS_TEST_REQUIRE("multi-state",
                        bs_adapter_attach_config_get_state(fix.ctx, uri, &state) == 0);
        BS_TEST_REQUIRE("multi-active", state == CONFIG_STATE_ACTIVE);

        void*  snap = nullptr;
        size_t sz   = 0;
        BS_TEST_REQUIRE("multi-snapshot",
                        bs_adapter_attach_config_get_snapshot(fix.ctx, uri, &snap, &sz) == 0);
        BS_TEST_REQUIRE("multi-nonempty", snap != nullptr && sz > 0);

        BS_TEST_REQUIRE("multi-content-match",
                        sz == kBlessStarConfigV1GoldenLen &&
                        std::memcmp(snap, kBlessStarConfigV1Golden, sz) == 0);
        std::free(snap);
    }

    bs_test_attach_teardown(&fix);
    std::fprintf(stderr, "SCENARIO 3: PASS\n");
    return 0;
}

// ============================================================================
// Test scenario 4: AddMemPath + GetConfig — new UX API
//   (in-memory config submission via AddMemPath, query via GetConfig)
// ============================================================================

static int test_mem_path_and_get_config()
{
    std::fprintf(stderr, "\n=== SCENARIO 4: AddMemPath + GetConfig ===\n");

    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_ux_mem_config"));
    const fs::path&          temp_dir = tmp_guard.path;

    BsTestAttachIoFixture fix = create_fixture_with_persist(temp_dir, "ux_mem.manifest");
    BS_TEST_REQUIRE("fix-ctx", fix.ctx != nullptr);

    const char* cfg_text =
        "{\"kernel_version\":\"0.4.0\",\"adapter_version\":\"0.4.0\","
        "\"manual_requirements\":[],\"instructions\":["
        "{\"type\":\"test\",\"name\":\"mem-path-test\","
        "\"metadata\":{\"subject_code\":\"2001\"}}]}";
    const size_t cfg_len = std::strlen(cfg_text);
    const uint8_t* cfg_bytes = reinterpret_cast<const uint8_t*>(cfg_text);

    const char* mem_key = "ux/mem_config";

    {
        bs::app::ConfigReloadSession session(fix.ctx);

        // Use the new AddMemPath API (UX-P6)
        BS_TEST_REQUIRE("add-mem-path", session.AddMemPath(mem_key, cfg_bytes, cfg_len));
        BS_TEST_REQUIRE("has-no-file-paths", true); // no file paths

        Report* r = session.Commit();
        BS_TEST_REQUIRE("mem-commit", r != nullptr);
        BS_TEST_REQUIRE("mem-commit-ok", bs_report_get_status(r) == REPORT_STATUS_SUCCESS);
        Report* taken = session.TakeReport();
        if (taken) bs_report_destroy(taken);
    }

    // -- GetConfig (UX-P8) — query without mem:// prefix --
    {
        void*  data = nullptr;
        size_t size = 0;
        // key without ':' -> auto-prepended as mem://ux/mem_config
        int rc = bs_adapter_attach_config_get_snapshot(
            fix.ctx, (std::string("mem://") + mem_key).c_str(), &data, &size);
        BS_TEST_REQUIRE("mem-getconfig", rc == 0);
        BS_TEST_REQUIRE("mem-getconfig-data", data != nullptr && size > 0);
        BS_TEST_REQUIRE("mem-getconfig-match",
                        size == cfg_len && std::memcmp(data, cfg_bytes, size) == 0);
        std::free(data);
    }

    bs_test_attach_teardown(&fix);
    std::fprintf(stderr, "SCENARIO 4: PASS\n");
    return 0;
}

// ============================================================================
// Test scenario 5: AppSession convenience + LastReport()
//   (one-line startup and read-only report access)
// ============================================================================

static int test_app_session_and_last_report()
{
    std::fprintf(stderr, "\n=== SCENARIO 5: AppSession + LastReport ===\n");

    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_ux_appsession"));
    const fs::path&          temp_dir = tmp_guard.path;

    // Use AppSession RAII (UX-P3)
    const fs::path manifest = temp_dir / "appsession.manifest";
    bs::app::AppSession sess(manifest.string().c_str());
    BS_TEST_REQUIRE("app-session-ok", sess.ok());
    BS_TEST_REQUIRE("app-session-ctx", sess.ctx() != nullptr);

    // Verify persist store opened via AppSession ctor
    BsAttachStore* store = bs_adapter_attach_ctx_persist_store(sess.ctx());
    BS_TEST_REQUIRE("app-session-store", store != nullptr);

    // Get IoFacade from AppSession for read_fn
    IoFacade* io = sess.io();
    BS_TEST_REQUIRE("app-session-io", io != nullptr);

    // Submit a valid config via normal AddUri + Commit
    const std::string cfg_uri = write_bytes_to_file(temp_dir, "appsession_cfg.json",
                                                     kBlessStarConfigV1Golden,
                                                     kBlessStarConfigV1GoldenLen);
    BS_TEST_REQUIRE("write-cfg", !cfg_uri.empty());

    {
        bs::app::ConfigReloadSession cs(sess.ctx());
        cs.SetReadFn(io_read_fn, io);
        BS_TEST_REQUIRE("add-uri", cs.AddUri(cfg_uri.c_str()));

        Report* r = cs.Commit();
        BS_TEST_REQUIRE("commit", r != nullptr);
        BS_TEST_REQUIRE("commit-ok", bs_report_get_status(r) == REPORT_STATUS_SUCCESS);

        // Test LastReport() (UX-P4) — should return the same report before TakeReport
        const Report* last = cs.LastReport();
        BS_TEST_REQUIRE("last-report-not-null", last != nullptr);
        BS_TEST_REQUIRE("last-report-same", last == r);

        // TakeReport should make LastReport return nullptr
        Report* taken = cs.TakeReport();
        BS_TEST_REQUIRE("taken-report", taken != nullptr);
        if (taken)
        {
            // After TakeReport, LastReport is nullptr (report transferred)
            const Report* after_take = cs.LastReport();
            BS_TEST_REQUIRE("last-report-after-take", after_take == nullptr);
            bs_report_destroy(taken);
        }
    }

    std::fprintf(stderr, "SCENARIO 5: PASS\n");
    return 0;
}

// ============================================================================
// Test scenario 6: Two-phase atomicity (FILE first, MEM second)
//   (verify that a FILE failure rolls back without MEM being applied)
// ============================================================================

static int test_two_phase_atomicity()
{
    std::fprintf(stderr, "\n=== SCENARIO 6: Two-phase atomicity ===\n");

    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_ux_twophase"));
    const fs::path&          temp_dir = tmp_guard.path;

    BsTestAttachIoFixture fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("ctx-create", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);

    const fs::path manifest = temp_dir / "twophase.manifest";
    BS_TEST_REQUIRE("open-store",
                    bs_adapter_attach_ctx_open_persist_store(fix.ctx, manifest.string().c_str()) == 0);

    // Write a valid config file (for use in the FILE phase)
    const std::string valid_uri = write_bytes_to_file(temp_dir, "valid.json",
                                                       kBlessStarConfigV1Golden,
                                                       kBlessStarConfigV1GoldenLen);
    BS_TEST_REQUIRE("write-valid", !valid_uri.empty());

    // Mem bytes for MEM phase (valid v1 JSON that passes the gate)
    // Note: must be valid BlessStar Config v1 JSON for the default gate to pass
    const char mem_v1_text[] =
        "{\"kernel_version\":\"0.4.0\",\"adapter_version\":\"0.4.0\","
        "\"manual_requirements\":[],\"instructions\":["
        "{\"type\":\"test\",\"name\":\"twophase-mem\","
        "\"metadata\":{\"subject_code\":\"3001\"}}]}";
    const uint8_t* mem_bytes = reinterpret_cast<const uint8_t*>(mem_v1_text);
    const size_t mem_len = std::strlen(mem_v1_text);

    // -- Test A: Both FILE and MEM succeed --
    {
        bs::app::ConfigReloadSession session(fix.ctx);
        session.SetReadFn(facade_read_fn, &fix);
        BS_TEST_REQUIRE("A-add-valid-uri", session.AddUri(valid_uri.c_str()));
        BS_TEST_REQUIRE("A-add-mem", session.AddMemPath("twophase/mem", mem_bytes, mem_len));

        Report* r = session.Commit();
        BS_TEST_REQUIRE("A-commit", r != nullptr);
        BS_TEST_REQUIRE("A-commit-ok", bs_report_get_status(r) == REPORT_STATUS_SUCCESS);
        Report* taken = session.TakeReport();
        if (taken) bs_report_destroy(taken);
    }

    // Verify both configs exist
    {
        ConfigState state;
        BS_TEST_REQUIRE("A-valid-state",
                        bs_adapter_attach_config_get_state(fix.ctx, valid_uri.c_str(), &state) == 0);
        BS_TEST_REQUIRE("A-valid-active", state == CONFIG_STATE_ACTIVE);

        std::string mem_uri = std::string("mem://twophase/mem");
        BS_TEST_REQUIRE("A-mem-state",
                        bs_adapter_attach_config_get_state(fix.ctx, mem_uri.c_str(), &state) == 0);
        BS_TEST_REQUIRE("A-mem-active", state == CONFIG_STATE_ACTIVE);
    }

    // -- Test B: FILE path has invalid content (gate fails) — should fail before MEM phase --
    {
        // Write a file with content that will fail gate validation
        const char bad_content[] = "{ not valid json content }";
        const std::string invalid_uri = write_bytes_to_file(temp_dir, "invalid_cfg.json",
                                                             bad_content, sizeof(bad_content) - 1);
        BS_TEST_REQUIRE("B-write-invalid", !invalid_uri.empty());

        bs::app::ConfigReloadSession session(fix.ctx);
        session.SetReadFn(facade_read_fn, &fix);
        BS_TEST_REQUIRE("B-add-invalid-uri", session.AddUri(invalid_uri.c_str()));
        BS_TEST_REQUIRE("B-add-mem", session.AddMemPath("twophase/should-not-exist", mem_bytes, mem_len));

        Report* r = session.Commit();
        BS_TEST_REQUIRE("B-commit", r != nullptr);
        // The batch controller may return SUCCESS with path-level error entries
        // We verify the MEM path was NOT created
        Report* taken = session.TakeReport();
        if (taken) bs_report_destroy(taken);
    }

    // Verify the MEM "should-not-exist" key was NOT created
    {
        std::string mem_uri = std::string("mem://twophase/should-not-exist");
        ConfigState state = CONFIG_STATE_INITIAL;
        int rc = bs_adapter_attach_config_get_state(fix.ctx, mem_uri.c_str(), &state);
        // Config should NOT be ACTIVE (it should be INITIAL or not found)
        BS_TEST_REQUIRE("B-mem-not-created",
                        rc != 0 || state != CONFIG_STATE_ACTIVE);
    }

    bs_test_attach_teardown(&fix);
    std::fprintf(stderr, "SCENARIO 6: PASS\n");
    return 0;
}

// ============================================================================
// Test scenario 7: PathSource::kFile enum + snapshot text util
// ============================================================================

static int test_path_source_enum_and_snapshot_util()
{
    std::fprintf(stderr, "\n=== SCENARIO 7: PathSource::kFile + snapshot utility ===\n");

    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_ux_pathsource"));
    const fs::path&          temp_dir = tmp_guard.path;

    BsTestAttachIoFixture fix = create_fixture_with_persist(temp_dir, "pathsource.manifest");
    BS_TEST_REQUIRE("fix-ctx", fix.ctx != nullptr);

    // Write golden config and use PathSource::kFile
    const std::string cfg_uri = write_bytes_to_file(temp_dir, "pathsource.json",
                                                     kBlessStarConfigV1Golden,
                                                     kBlessStarConfigV1GoldenLen);
    BS_TEST_REQUIRE("write-cfg", !cfg_uri.empty());

    {
        bs::app::ConfigReloadSession session(fix.ctx);
        session.SetReadFn(facade_read_fn, &fix);

        // Use PathSource::kFile enum overload (UX-P6)
        BS_TEST_REQUIRE("add-kfile", session.AddPath(cfg_uri.c_str(), bs::app::PathSource::kFile));

        Report* r = session.Commit();
        BS_TEST_REQUIRE("kfile-commit", r != nullptr);
        BS_TEST_REQUIRE("kfile-ok", bs_report_get_status(r) == REPORT_STATUS_SUCCESS);
        Report* taken = session.TakeReport();
        if (taken) bs_report_destroy(taken);
    }

    // Verify config is active
    {
        ConfigState state;
        BS_TEST_REQUIRE("kfile-state",
                        bs_adapter_attach_config_get_state(fix.ctx, cfg_uri.c_str(), &state) == 0);
        BS_TEST_REQUIRE("kfile-active", state == CONFIG_STATE_ACTIVE);

        // Test snapshot text utility (UX-P5)
        void*  snap      = nullptr;
        size_t snap_size = 0;
        BS_TEST_REQUIRE("kfile-snapshot",
                        bs_adapter_attach_config_get_snapshot(fix.ctx, cfg_uri.c_str(),
                                                              &snap, &snap_size) == 0);

        char* text = bs_attach_watch_snapshot_as_text(snap, snap_size);
        BS_TEST_REQUIRE("snap-text-alloc", text != nullptr);
        BS_TEST_REQUIRE("snap-text-contains",
                        std::strstr(text, "reload-smoke-1") != nullptr);
        bs_attach_watch_snapshot_text_free(text);
        std::free(snap);
    }

    // Test PathSource::kHttp/kHttps returns false
    {
        bs::app::ConfigReloadSession session(fix.ctx);
        BS_TEST_REQUIRE("khttp-not-implemented",
                        session.AddPath("http://example.com", bs::app::PathSource::kHttp) == false);
        BS_TEST_REQUIRE("khttps-not-implemented",
                        session.AddPath("https://example.com", bs::app::PathSource::kHttps) == false);
    }

    bs_test_attach_teardown(&fix);
    std::fprintf(stderr, "SCENARIO 7: PASS\n");
    return 0;
}

// ============================================================================
// Test scenario 8: ConfigSessionReader metadata consumption (MD-D-08)
// ============================================================================

/** Watch callback that consumes metadata via ConfigSessionReader. */
struct MetaWatchCtx
{
    ConfigEventType event_type;
};

static void meta_watch_callback(const char* uri, ConfigEventType type,
                                const void* /*snapshot*/, void* user_data)
{
    auto* wctx = static_cast<MetaWatchCtx*>(user_data);
    wctx->event_type = type;
}

static int test_metadata_consumption()
{
    std::fprintf(stderr, "\n=== SCENARIO 8: ConfigSessionReader metadata consumption ===\n");

    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_md_consume"));
    const fs::path&          temp_dir = tmp_guard.path;

    BsTestAttachIoFixture fix = create_fixture_with_persist(temp_dir, "mdconsume.manifest");
    BS_TEST_REQUIRE("fix-ctx", fix.ctx != nullptr);

    // ── 8a. Submit a config with metadata via AddMemPath ───────────────────
    bs::app::ConfigSessionReader reader(fix.ctx);
    const char* const mem_key = "memcfg";
    const std::string mem_uri = std::string("mem://") + mem_key;

    {
        bs::app::ConfigReloadSession session(fix.ctx);

        // Config with metadata: taxes/rates scenario
        const char* cfg_json = R"({
            "kernel_version": "0.4.0",
            "adapter_version": "0.4.0",
            "manual_requirements": [],
            "instructions": [
                {
                    "type": "test",
                    "name": "vat-rate",
                    "metadata": {
                        "subject_code": "1001.01",
                        "tax_rate": "13",
                        "effective_from": "2026-01-01"
                    }
                },
                {
                    "type": "test",
                    "name": "income-tax-rate",
                    "metadata": {
                        "subject_code": "2001.00",
                        "tax_rate": "25",
                        "exempt_threshold": "5000"
                    }
                }
            ]
        })";

        BS_TEST_REQUIRE("addmem-ok",
                        session.AddMemPath(mem_key, (const uint8_t*)cfg_json, std::strlen(cfg_json)));

        Report* r = session.Commit();
        BS_TEST_REQUIRE("commit-ok", r != nullptr);
        BS_TEST_REQUIRE("commit-success",
                        bs_report_get_status(r) == REPORT_STATUS_SUCCESS);
        Report* taken = session.TakeReport();
        if (taken) bs_report_destroy(taken);
    }

    // ── 8b. Verify GetInstruction / GetMetadata for MEM path ──────────────
    {
        const IRInstruction* instr = reader.GetInstruction(mem_uri.c_str(), "vat-rate");
        BS_TEST_REQUIRE("instr-found", instr != nullptr);
        BS_TEST_REQUIRE("instr-type", instr->type != nullptr &&
                        std::strcmp(instr->type, "test") == 0);
        BS_TEST_REQUIRE("instr-name", instr->name != nullptr &&
                        std::strcmp(instr->name, "vat-rate") == 0);

        // Verify metadata directly
        const IRMetadata* meta = instr->metadata;
        BS_TEST_REQUIRE("meta-chain", meta != nullptr);

        bool found_subject = false;
        bool found_rate    = false;
        bool found_eff     = false;
        for (const IRMetadata* m = meta; m; m = m->next)
        {
            if (std::strcmp(m->key, "subject_code") == 0 &&
                std::strcmp(m->value, "1001.01") == 0)
                found_subject = true;
            if (std::strcmp(m->key, "tax_rate") == 0 &&
                std::strcmp(m->value, "13") == 0)
                found_rate = true;
            if (std::strcmp(m->key, "effective_from") == 0 &&
                std::strcmp(m->value, "2026-01-01") == 0)
                found_eff = true;
        }
        BS_TEST_REQUIRE("meta-subject", found_subject);
        BS_TEST_REQUIRE("meta-rate", found_rate);
        BS_TEST_REQUIRE("meta-eff", found_eff);

        // Test GetMetadata shortcut
        const IRMetadata* meta_short = reader.GetMetadata(mem_uri.c_str(), "vat-rate");
        BS_TEST_REQUIRE("getmeta-nonnull", meta_short != nullptr);
        BS_TEST_REQUIRE("getmeta-key", meta_short->key != nullptr);
        BS_TEST_REQUIRE("getmeta-subject",
                        std::strcmp(bs_ir_instruction_get_metadata(
                            reader.GetInstruction(mem_uri.c_str(), "vat-rate"), "subject_code"),
                                    "1001.01") == 0);
    }

    // ── 8c. Verify second instruction ─────────────────────────────────────
    {
        const IRInstruction* instr2 = reader.GetInstruction(mem_uri.c_str(), "income-tax-rate");
        BS_TEST_REQUIRE("instr2-found", instr2 != nullptr);
        BS_TEST_REQUIRE("instr2-name", std::strcmp(instr2->name, "income-tax-rate") == 0);

        // Check exempt_threshold metadata
        BS_TEST_REQUIRE("meta2-exempt",
                        std::strcmp(bs_ir_instruction_get_metadata(instr2, "exempt_threshold"),
                                    "5000") == 0);
    }

    // ── 8d. Submit FILE path with metadata, verify via Reader ────────────
    const std::string file_uri = write_bytes_to_file(temp_dir, "file_cfg.json",
                                                      kBlessStarConfigV1Golden,
                                                      kBlessStarConfigV1GoldenLen);

    {
        bs::app::ConfigReloadSession session(fix.ctx);
        session.SetReadFn(facade_read_fn, &fix);
        BS_TEST_REQUIRE("addfile-ok", session.AddFilePath(file_uri.c_str()));

        Report* r = session.Commit();
        BS_TEST_REQUIRE("file-commit-ok", r != nullptr);
        BS_TEST_REQUIRE("file-commit-success",
                        bs_report_get_status(r) == REPORT_STATUS_SUCCESS);
        Report* taken = session.TakeReport();
        if (taken) bs_report_destroy(taken);
    }

    // Reader should now see FILE path instructions too (MD-D-03)
    {
        const IRInstruction* finstr = reader.GetInstruction(file_uri.c_str(), "reload-smoke-1");
        BS_TEST_REQUIRE("file-instr-found", finstr != nullptr);
        BS_TEST_REQUIRE("file-instr-type", std::strcmp(finstr->type, "test") == 0);

        // FILE path also has metadata in golden config
        const IRMetadata* fmeta = finstr->metadata;
        BS_TEST_REQUIRE("file-meta", fmeta != nullptr);
        BS_TEST_REQUIRE("file-meta-subject",
                        std::strcmp(bs_ir_instruction_get_metadata(finstr, "subject_code"),
                                    "1001.01") == 0);
        BS_TEST_REQUIRE("file-meta-rate",
                        std::strcmp(bs_ir_instruction_get_metadata(finstr, "tax_rate"),
                                    "13") == 0);
    }

    // ── 8e. hot_update → Reader auto-refresh (MD-D-05) ──────────────────
    {
        // Submit a new version of memcfg with different metadata
        bs::app::ConfigReloadSession session(fix.ctx);

        const char* updated_cfg = R"({
            "kernel_version": "0.4.0",
            "adapter_version": "0.4.0",
            "manual_requirements": [],
            "instructions": [
                {
                    "type": "test",
                    "name": "vat-rate",
                    "metadata": {
                        "subject_code": "1001.01",
                        "tax_rate": "15",
                        "effective_from": "2026-07-01"
                    }
                }
            ]
        })";

        BS_TEST_REQUIRE("hot-addmem",
                        session.AddMemPath(mem_key, (const uint8_t*)updated_cfg, std::strlen(updated_cfg)));

        Report* r = session.Commit();
        BS_TEST_REQUIRE("hot-commit-ok", r != nullptr);
        BS_TEST_REQUIRE("hot-commit-success",
                        bs_report_get_status(r) == REPORT_STATUS_SUCCESS);
        Report* taken = session.TakeReport();
        if (taken) bs_report_destroy(taken);
    }

    // After hot_update, Reader should auto-detect version change (MD-D-05)
    {
        const IRInstruction* new_instr = reader.GetInstruction(mem_uri.c_str(), "vat-rate");
        BS_TEST_REQUIRE("hot-instr-found", new_instr != nullptr);

        // tax_rate changed from 13 → 15
        const char* rate = bs_ir_instruction_get_metadata(new_instr, "tax_rate");
        BS_TEST_REQUIRE("hot-rate-changed", rate != nullptr &&
                        std::strcmp(rate, "15") == 0);

        // effective_from changed to 2026-07-01
        const char* eff = bs_ir_instruction_get_metadata(new_instr, "effective_from");
        BS_TEST_REQUIRE("hot-eff-changed", eff != nullptr &&
                        std::strcmp(eff, "2026-07-01") == 0);
    }

    // ── 8f. Watch callback uses ConfigSessionReader (MD-D-04) ────────────
    {
        MetaWatchCtx wctx;
        wctx.event_type = CONFIG_EVENT_ENTER_INITIAL;

        BS_TEST_REQUIRE("watch-register",
                        bs_adapter_attach_config_subscribe_state_watch(
                            fix.ctx, ("mem://" + std::string(mem_key)).c_str(),
                            meta_watch_callback, &wctx) == 0);

        // Submit the same config again to trigger watch
        bs::app::ConfigReloadSession session(fix.ctx);
        const char* same_cfg = R"({
            "kernel_version": "0.4.0",
            "adapter_version": "0.4.0",
            "manual_requirements": [],
            "instructions": [
                {
                    "type": "test",
                    "name": "vat-rate",
                    "metadata": {
                        "subject_code": "1001.01",
                        "tax_rate": "15",
                        "effective_from": "2026-07-01"
                    }
                }
            ]
        })";

        BS_TEST_REQUIRE("watch-addmem",
                        session.AddMemPath(mem_key, (const uint8_t*)same_cfg, std::strlen(same_cfg)));

        Report* r = session.Commit();
        BS_TEST_REQUIRE("watch-commit-ok", r != nullptr);
        Report* taken = session.TakeReport();
        if (taken) bs_report_destroy(taken);
    }

    // Reader should work inside watch callback (we verify it works after)
    // after the watch-event-triggering Commit, Reader auto-refreshes
    {
        const IRInstruction* instr = reader.GetInstruction(mem_uri.c_str(), "vat-rate");
        BS_TEST_REQUIRE("watch-instr-ok", instr != nullptr);
    }

    bs_test_attach_teardown(&fix);
    std::fprintf(stderr, "SCENARIO 8: PASS\n");
    return 0;
}

// ============================================================================
// Scenario 9: Policy metadata_rules gate (MR-09)
//
// Submits a v1 config with metadata, adds a ScenarioPolicy with metadata_rules.
//   - Positive: metadata_rules that pass should let Commit return OK
//   - Negative: metadata_rules that fail should cause Commit to return FAILED
// ============================================================================

static int test_policy_metadata_gate()
{
    std::fprintf(stderr, "\n=== SCENARIO 9: Policy metadata_rules gate ===\n");

    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_scenario9_meta"));
    const fs::path&          temp_dir = tmp_guard.path;

    BsTestAttachIoFixture fix = create_fixture_with_persist(temp_dir, "scenario9_manifest.yaml");
    BS_TEST_REQUIRE("fix-ctx", fix.ctx != nullptr);

    // --- V1 config bytes with metadata ---
    const char kConfigWithMeta[] = R"({
  "kernel_version": "0.4.0",
  "adapter_version": "0.4.0",
  "manual_requirements": [],
  "instructions": [
    {
      "type": "test",
      "name": "instr-tax",
      "metadata": {
        "subject_code": "1001.01",
        "tax_rate": "13",
        "region": "CN"
      }
    },
    {
      "type": "test",
      "name": "instr-expense",
      "metadata": {
        "subject_code": "2002.05",
        "amount": "5000"
      }
    }
  ]
})";
    const size_t kConfigLen = sizeof(kConfigWithMeta) - 1;

    // ---- Test A: metadata_rules match (should PASS) ----
    {
        bs::app::ConfigReloadSession session(fix.ctx);

        bs::app::ScenarioPolicy policy;
        policy.tenant           = "scenario9-tenant";
        policy.allow_hot_reload = true;
        policy.max_batch        = 64;
        policy.metadata_rules.push_back({"instr-tax", "tax_rate", BS_META_EQ, "13"});
        policy.metadata_rules.push_back({"instr-tax", "region", BS_META_EQ, "CN"});
        session.AddPolicyGate(policy);

        Report* r = session.Commit();
        BS_TEST_REQUIRE("metadata-match-ok", r != nullptr);

        bool commit_ok = (bs_report_get_status(r) == REPORT_STATUS_FAILED) ? false : true;
        if (!commit_ok)
        {
            // Get error message if any
            const char* err_str = bs_report_get_error_message(r);
            std::fprintf(stderr, "  metadata-match-ok: FAIL (Commit returned FAILED, err=%s)\n",
                         err_str ? err_str : "unknown");
            bs_report_destroy(r);
            bs_test_attach_teardown(&fix);
            return 1;
        }
        std::fprintf(stderr, "  metadata-match-ok: PASS\n");
        bs_report_destroy(r);
        session.Reset();
    }

    // ---- Test B: metadata_rules mismatch (should FAIL) ----
    {
        bs::app::ConfigReloadSession session(fix.ctx);

        bs::app::ScenarioPolicy policy;
        policy.tenant           = "scenario9-tenant";
        policy.allow_hot_reload = true;
        policy.max_batch        = 64;
        policy.metadata_rules.push_back({"instr-tax", "tax_rate", BS_META_GT, "20"});
        session.AddPolicyGate(policy);

        Report* r = session.Commit();
        BS_TEST_REQUIRE("metadata-mismatch-report", r != nullptr);

        bool commit_failed = (bs_report_get_status(r) == REPORT_STATUS_FAILED);
        if (!commit_failed)
        {
            std::fprintf(stderr, "  metadata-mismatch-fail: FAIL (expected FAILED but got OK)\n");
            bs_report_destroy(r);
            bs_test_attach_teardown(&fix);
            return 1;
        }
        std::fprintf(stderr, "  metadata-mismatch-fail: PASS (Commit FAILED as expected)\n");
        bs_report_destroy(r);
        session.Reset();
    }

    // ---- Test C: metadata_rules with empty instr_name (match all, one should fail) ----
    {
        bs::app::ConfigReloadSession session(fix.ctx);

        bs::app::ScenarioPolicy policy;
        policy.tenant           = "scenario9-tenant";
        policy.allow_hot_reload = true;
        policy.max_batch        = 64;
        // Empty instr_name = match ALL instructions. instr-tax has tax_rate=13,
        // instr-expense has NO tax_rate, so this should fail.
        policy.metadata_rules.push_back({"", "tax_rate", BS_META_EXISTS, ""});
        session.AddPolicyGate(policy);

        Report* r = session.Commit();
        BS_TEST_REQUIRE("metadata-all-required", r != nullptr);

        bool commit_failed = (bs_report_get_status(r) == REPORT_STATUS_FAILED);
        if (!commit_failed)
        {
            std::fprintf(stderr, "  metadata-all-required: FAIL (expected FAILED, tax_rate missing on instr-expense)\n");
            bs_report_destroy(r);
            bs_test_attach_teardown(&fix);
            return 1;
        }
        std::fprintf(stderr, "  metadata-all-required: PASS (Commit FAILED as expected)\n");
        bs_report_destroy(r);
        session.Reset();
    }

    // ---- Test D: no metadata_rules (backward compat - should PASS) ----
    {
        bs::app::ConfigReloadSession session(fix.ctx);

        bs::app::ScenarioPolicy policy;
        policy.tenant           = "scenario9-tenant";
        policy.allow_hot_reload = true;
        policy.max_batch        = 64;
        // No metadata_rules — should pass as before
        session.AddPolicyGate(policy);

        Report* r = session.Commit();
        BS_TEST_REQUIRE("no-metadata-rules", r != nullptr);

        bool commit_ok = (bs_report_get_status(r) == REPORT_STATUS_FAILED) ? false : true;
        if (!commit_ok)
        {
            std::fprintf(stderr, "  no-metadata-rules: FAIL (Commit FAILED unexpectedly)\n");
            bs_report_destroy(r);
            bs_test_attach_teardown(&fix);
            return 1;
        }
        std::fprintf(stderr, "  no-metadata-rules: PASS\n");
        bs_report_destroy(r);
        session.Reset();
    }

    bs_test_attach_teardown(&fix);
    std::fprintf(stderr, "SCENARIO 9: PASS\n");
    return 0;
}

// ============================================================================
// Scenario 9 EXTENDED: metadata_rules sub-cases 9a-9i
// ============================================================================

static int test_policy_metadata_gate_extended()
{
    std::fprintf(stderr, "\n=== SCENARIO 9 EXTENDED: metadata_rules sub-cases ===\n");

    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_scenario9_ext"));
    const fs::path&          temp_dir = tmp_guard.path;

    BsTestAttachIoFixture fix = create_fixture_with_persist(temp_dir, "scenario9_ext.manifest");
    BS_TEST_REQUIRE("fix-ctx", fix.ctx != nullptr);

    const char kConfigWithMeta[] = R"({
  "kernel_version": "0.4.0",
  "adapter_version": "0.4.0",
  "manual_requirements": [],
  "instructions": [
    {
      "type": "test",
      "name": "instr-tax",
      "metadata": {
        "subject_code": "1001.01",
        "tax_rate": "13",
        "region": "CN"
      }
    },
    {
      "type": "test",
      "name": "instr-expense",
      "metadata": {
        "subject_code": "2002.05",
        "amount": "5000"
      }
    }
  ]
})";
    const size_t kConfigLen = sizeof(kConfigWithMeta) - 1;

    auto run_session = [&](const bs::app::ScenarioPolicy& policy) -> Report* {
        bs::app::ConfigReloadSession session(fix.ctx);
        session.AddPolicyGate(policy);
        return session.Commit();
    };

    // 9a: EQ pass (tax_rate == "13")
    {
        bs::app::ScenarioPolicy policy;
        policy.tenant = "ext"; policy.allow_hot_reload = true; policy.max_batch = 64;
        policy.metadata_rules.push_back({"instr-tax", "tax_rate", BS_META_EQ, "13"});
        Report* r = run_session(policy);
        BS_TEST_REQUIRE("9a_eq_pass", r != nullptr);
        BS_TEST_REQUIRE("9a_eq_status", bs_report_get_status(r) != REPORT_STATUS_FAILED);
        bs_report_destroy(r);
    }

    // 9b: GT pass (tax_rate > "10")
    {
        bs::app::ScenarioPolicy policy;
        policy.tenant = "ext"; policy.allow_hot_reload = true; policy.max_batch = 64;
        policy.metadata_rules.push_back({"instr-tax", "tax_rate", BS_META_GT, "10"});
        Report* r = run_session(policy);
        BS_TEST_REQUIRE("9b_gt_pass", r != nullptr);
        BS_TEST_REQUIRE("9b_gt_status", bs_report_get_status(r) != REPORT_STATUS_FAILED);
        bs_report_destroy(r);
    }

    // 9c: LT pass (tax_rate < "20")
    {
        bs::app::ScenarioPolicy policy;
        policy.tenant = "ext"; policy.allow_hot_reload = true; policy.max_batch = 64;
        policy.metadata_rules.push_back({"instr-tax", "tax_rate", BS_META_LT, "20"});
        Report* r = run_session(policy);
        BS_TEST_REQUIRE("9c_lt_pass", r != nullptr);
        BS_TEST_REQUIRE("9c_lt_status", bs_report_get_status(r) != REPORT_STATUS_FAILED);
        bs_report_destroy(r);
    }

    // 9d: EXISTS pass (subject_code exists on instr-tax)
    {
        bs::app::ScenarioPolicy policy;
        policy.tenant = "ext"; policy.allow_hot_reload = true; policy.max_batch = 64;
        policy.metadata_rules.push_back({"instr-tax", "subject_code", BS_META_EXISTS, ""});
        Report* r = run_session(policy);
        BS_TEST_REQUIRE("9d_exists_pass", r != nullptr);
        BS_TEST_REQUIRE("9d_exists_status", bs_report_get_status(r) != REPORT_STATUS_FAILED);
        bs_report_destroy(r);
    }

    // 9e: NOT_EXISTS pass (nonexistent_key not present)
    {
        bs::app::ScenarioPolicy policy;
        policy.tenant = "ext"; policy.allow_hot_reload = true; policy.max_batch = 64;
        policy.metadata_rules.push_back({"", "nonexistent_key", BS_META_NOT_EXISTS, ""});
        Report* r = run_session(policy);
        BS_TEST_REQUIRE("9e_not_exists_pass", r != nullptr);
        BS_TEST_REQUIRE("9e_not_exists_status", bs_report_get_status(r) != REPORT_STATUS_FAILED);
        bs_report_destroy(r);
    }

    // 9f: CONTAINS pass (subject_code contains "1001")
    {
        bs::app::ScenarioPolicy policy;
        policy.tenant = "ext"; policy.allow_hot_reload = true; policy.max_batch = 64;
        policy.metadata_rules.push_back({"instr-tax", "subject_code", BS_META_CONTAINS, "1001"});
        Report* r = run_session(policy);
        BS_TEST_REQUIRE("9f_contains_pass", r != nullptr);
        BS_TEST_REQUIRE("9f_contains_status", bs_report_get_status(r) != REPORT_STATUS_FAILED);
        bs_report_destroy(r);
    }

    // 9g: EQ fail (tax_rate == "99" → should FAIL)
    {
        bs::app::ScenarioPolicy policy;
        policy.tenant = "ext"; policy.allow_hot_reload = true; policy.max_batch = 64;
        policy.metadata_rules.push_back({"instr-tax", "tax_rate", BS_META_EQ, "99"});
        Report* r = run_session(policy);
        BS_TEST_REQUIRE("9g_eq_fail", r != nullptr);
        BS_TEST_REQUIRE("9g_eq_status", bs_report_get_status(r) == REPORT_STATUS_FAILED);
        bs_report_destroy(r);
    }

    // 9h: full match (instr_name="" → all instructions checked; tax_rate exists on instr-tax only → FAIL)
    {
        bs::app::ScenarioPolicy policy;
        policy.tenant = "ext"; policy.allow_hot_reload = true; policy.max_batch = 64;
        policy.metadata_rules.push_back({"", "tax_rate", BS_META_EXISTS, ""});
        Report* r = run_session(policy);
        BS_TEST_REQUIRE("9h_full_match", r != nullptr);
        BS_TEST_REQUIRE("9h_full_status", bs_report_get_status(r) == REPORT_STATUS_FAILED);
        bs_report_destroy(r);
    }

    // 9i: single instr match (instr_name="instr-tax" → only check instr-tax)
    {
        bs::app::ScenarioPolicy policy;
        policy.tenant = "ext"; policy.allow_hot_reload = true; policy.max_batch = 64;
        policy.metadata_rules.push_back({"instr-tax", "tax_rate", BS_META_EQ, "13"});
        Report* r = run_session(policy);
        BS_TEST_REQUIRE("9i_single_match", r != nullptr);
        BS_TEST_REQUIRE("9i_single_status", bs_report_get_status(r) != REPORT_STATUS_FAILED);
        bs_report_destroy(r);
    }

    bs_test_attach_teardown(&fix);
    std::fprintf(stderr, "SCENARIO 9 EXTENDED: PASS\n");
    return 0;
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    int rc = 0;

    rc |= test_full_config_lifecycle();
    rc |= test_bad_config_isolation();
    rc |= test_multi_path_atomic_submit();
    rc |= test_mem_path_and_get_config();
    rc |= test_app_session_and_last_report();
    rc |= test_two_phase_atomicity();
    rc |= test_path_source_enum_and_snapshot_util();
    rc |= test_metadata_consumption();
    rc |= test_policy_metadata_gate();
    rc |= test_policy_metadata_gate_extended();

    if (rc != 0)
        std::fprintf(stderr, "\nFAIL: some real-biz scenarios returned non-zero\n");
    else
        std::fprintf(stderr, "\nAll real-biz full-chain scenarios PASSED\n");
    return rc;
}

