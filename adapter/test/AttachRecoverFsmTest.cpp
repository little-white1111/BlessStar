/**
 * Day 22 Phase 2 / REC-A'-7: EXEC rollback FSM + per_batch PHASE_MARK (T-REC.6-10).
 */

#include "bs/kernel/report/report.h"

#include "bs/adapter/attach_context.h"
#include "bs/adapter/attach_ir_snapshot.h"
#include "bs/adapter/attach_recover.h"
#include "bs/adapter/attach_session.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/persistence/attach_store.h"
#include "bs/adapter/persistence/attach_wal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <string>

#include "support/attach_test_fixture.h"
#include "support/config_v1_golden.h"
#include "support/test_temp_dir.h"

namespace fs = std::filesystem;

static int golden_read(void*, const char*, IoReadResult* out)
{
    bs_io_read_result_init(out);
    out->status = BS_IO_OK;
    out->length = kBlessStarConfigV1GoldenLen;
    out->data   = static_cast<uint8_t*>(std::malloc(out->length));
    if (!out->data)
        return BS_IO_ERR_PROVIDER;
    std::memcpy(out->data, kBlessStarConfigV1Golden, out->length);
    return BS_IO_OK;
}

static int prime_manifest(const fs::path& config_path, const fs::path& manifest_path,
                          std::string* uri_out)
{
    const std::string uri   = bs_test_path_to_file_uri(config_path);
    BsAttachStore*    store = bs_adapter_attach_persist_store_open(manifest_path.string().c_str());
    BS_TEST_REQUIRE("prime-store", store != nullptr);
    BS_TEST_REQUIRE("prime-commit", bs_adapter_attach_persist_store_commit_per_path(
                                        store, uri.c_str(), kBlessStarConfigV1Golden,
                                        kBlessStarConfigV1GoldenLen, 0) == BS_ATTACH_OK);
    const uint64_t epoch = bs_adapter_attach_persist_store_batch_epoch(store);
    BS_TEST_REQUIRE("prime-epoch", epoch >= 1);
    bs_adapter_attach_persist_store_close(store);
    *uri_out = uri;
    return 0;
}

static int test_exec_rollback_manifest_epoch_unchanged(const fs::path&    cfg,
                                                       const fs::path&    manifest,
                                                       const std::string& uri)
{
    (void)cfg;
    BsTestAttachIoFixture fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("ctx", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("p2", bs_registry_facade_advance_phase(fix.facade, BS_REGISTRY_PHASE_P2) ==
                              BS_REGISTRY_OK);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("pool", bs_adapter_attach_ctx_is_kernel_pool_warmed(fix.ctx) == 1);
    bs_adapter_attach_ctx_set_active(fix.ctx);

    BsAttachStore* probe = bs_adapter_attach_persist_store_open(manifest.string().c_str());
    BS_TEST_REQUIRE("probe", probe != nullptr);
    const uint64_t epoch_before = bs_adapter_attach_persist_store_batch_epoch(probe);
    bs_adapter_attach_persist_store_close(probe);

    bs_adapter_attach_reload_batch_testing_set_abort_after_exec(1);

    ReloadBatchController* ctrl = bs_adapter_attach_reload_batch_create(4);
    BS_TEST_REQUIRE("ctrl", ctrl != nullptr);
    bs_adapter_attach_reload_batch_set_read_fn(ctrl, golden_read, nullptr);
    bs_adapter_attach_reload_batch_set_default_gate(ctrl);
    bs_adapter_attach_reload_batch_set_attach_ctx(ctrl, fix.ctx);
    bs_adapter_attach_reload_batch_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_BATCH);
    bs_adapter_attach_reload_batch_set_manifest_path(ctrl, manifest.string().c_str());
    BS_TEST_REQUIRE("add", bs_adapter_attach_reload_batch_add_path(ctrl, uri.c_str()) == 0);

    const int run_rc = bs_adapter_attach_reload_batch_run(ctrl);
    BS_TEST_REQUIRE("run-fail", run_rc != 0);
    BS_TEST_REQUIRE("outcome-fail",
                    bs_adapter_attach_reload_batch_outcome(ctrl) == BATCH_COMPLETED_WITH_FAILURES);
    BS_TEST_REQUIRE("ir-empty", bs_adapter_attach_ir_snapshot_entry_count(fix.ctx) == 0);

    bs_adapter_attach_reload_batch_testing_set_abort_after_exec(0);
    bs_adapter_attach_reload_batch_destroy(ctrl);

    probe = bs_adapter_attach_persist_store_open(manifest.string().c_str());
    BS_TEST_REQUIRE("probe2", probe != nullptr);
    BS_TEST_REQUIRE("epoch-unchanged",
                    bs_adapter_attach_persist_store_batch_epoch(probe) == epoch_before);
    BS_TEST_REQUIRE("exec-rollback",
                    bs_adapter_attach_persist_store_had_exec_rollback(probe, nullptr) == 1);
    bs_adapter_attach_persist_store_close(probe);

    AttachContext* rec = bs_adapter_attach_recover_from_store(manifest.string().c_str(), nullptr);
    BS_TEST_REQUIRE("recover-step1", rec != nullptr);
    bs_adapter_attach_ctx_set_active(rec);

    BsTestAttachIoFixture rec_fix{};
    rec_fix.ctx = rec;
    BS_TEST_REQUIRE("rec-bootstrap", bs_test_attach_bootstrap_begin_ctx(&rec_fix) == 0);
    BS_TEST_REQUIRE("rec-freeze", bs_test_attach_bootstrap_freeze_ctx(&rec_fix) == 0);
    BS_TEST_REQUIRE("rec-io", bs_test_attach_open_io(&rec_fix) == 0);

    Report* report = bs_report_create("recover_fsm_cold");
    BS_TEST_REQUIRE("recover-report", report != nullptr);

    BsAttachRecoverColdReloadOptions opts{};
    opts.struct_size               = sizeof(opts);
    const std::string manifest_str = manifest.string();
    opts.manifest_path             = manifest_str.c_str();
    opts.io_facade                 = rec_fix.io;
    opts.scheme                    = BS_ATTACH_SCHEME_PER_BATCH;
    opts.max_inflight              = 4;
    opts.report                    = report;

    const int recover_rc = bs_adapter_attach_recover_cold_reload(rec, &opts);
    if (recover_rc != 0)
    {
        char* json = bs_report_to_json(report);
        std::fprintf(stderr, "recover-step2 rc=%d recovering=%d report=%s\n", recover_rc,
                     bs_adapter_attach_session_is_recovering(rec), json ? json : "<null>");
        std::free(json);
    }
    bs_report_destroy(report);
    BS_TEST_REQUIRE("recover-step2", recover_rc == 0);
    BS_TEST_REQUIRE("ready", bs_adapter_attach_session_is_recovering(rec) == 0);

    bs_test_attach_teardown(&rec_fix);
    bs_test_attach_teardown(&fix);
    return 0;
}

static int test_per_path_has_no_phase_mark_wal(const fs::path& cfg, const fs::path& manifest,
                                               const std::string& uri)
{
    (void)cfg;
    BsTestAttachIoFixture fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("ctx", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    bs_adapter_attach_ctx_set_active(fix.ctx);

    ReloadBatchController* ctrl = bs_adapter_attach_reload_batch_create(4);
    BS_TEST_REQUIRE("ctrl", ctrl != nullptr);
    bs_adapter_attach_reload_batch_set_read_fn(ctrl, golden_read, nullptr);
    bs_adapter_attach_reload_batch_set_default_gate(ctrl);
    bs_adapter_attach_reload_batch_set_attach_ctx(ctrl, fix.ctx);
    bs_adapter_attach_reload_batch_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_PATH);
    bs_adapter_attach_reload_batch_set_manifest_path(ctrl, manifest.string().c_str());
    BS_TEST_REQUIRE("add", bs_adapter_attach_reload_batch_add_path(ctrl, uri.c_str()) == 0);
    BS_TEST_REQUIRE("run", bs_adapter_attach_reload_batch_run(ctrl) == 0);
    BS_TEST_REQUIRE("ok", bs_adapter_attach_reload_batch_outcome(ctrl) == BATCH_ALL_OK);
    bs_adapter_attach_reload_batch_destroy(ctrl);

    const std::string wal_path = manifest.string() + ".wal";
    BsAttachWal*      wal      = bs_adapter_attach_persist_wal_open(wal_path.c_str());
    BS_TEST_REQUIRE("wal", wal != nullptr);
    size_t phase_marks = 0;
    BS_TEST_REQUIRE("count", bs_adapter_attach_persist_wal_count_record_type(
                                 wal, 5, &phase_marks) == BS_ATTACH_OK);
    BS_TEST_REQUIRE("no-phase-mark", phase_marks == 0);
    bs_adapter_attach_persist_wal_close(wal);

    bs_test_attach_teardown(&fix);
    return 0;
}

int main()
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_attach_recover_fsm"));
    const fs::path           work     = tmp_guard.path;
    const fs::path           cfg      = work / "recover_fsm.json";
    const fs::path           manifest = work / "manifest_fsm.bs";

    BS_TEST_REQUIRE("write", bs_test_write_binary_file(cfg, kBlessStarConfigV1Golden,
                                                       kBlessStarConfigV1GoldenLen));

    std::string uri;
    BS_TEST_REQUIRE("prime", prime_manifest(cfg, manifest, &uri) == 0);
    BS_TEST_REQUIRE("exec-rollback",
                    test_exec_rollback_manifest_epoch_unchanged(cfg, manifest, uri) == 0);
    BS_TEST_REQUIRE("scheme-guard", test_per_path_has_no_phase_mark_wal(cfg, manifest, uri) == 0);

    std::fprintf(stderr, "AttachRecoverFsmTest: PASS\n");
    return 0;
}
