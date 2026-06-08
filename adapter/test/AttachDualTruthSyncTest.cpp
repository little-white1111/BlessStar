/**
 * REC-G-03: manifest revision is the sole authority after config_sync.
 */

#include "bs/adapter/attach_config.h"
#include "bs/adapter/attach_errors.h"
#include "bs/adapter/attach_ir_snapshot.h"
#include "bs/adapter/attach_session.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/persistence/attach_store.h"

#include <cstdio>

#include <filesystem>
#include <string>
#include <vector>

#include "support/attach_test_fixture.h"
#include "support/config_v1_golden.h"
#include "support/test_temp_dir.h"

namespace fs = std::filesystem;

static int facade_read_fn(void* user_ctx, const char* uri, IoReadResult* out)
{
    auto* fix = static_cast<BsTestAttachIoFixture*>(user_ctx);
    return bs_io_facade_read(fix->io, uri, out);
}

static int run_reload(BsTestAttachIoFixture* fix, const fs::path& manifest, const std::string& uri)
{
    ReloadBatchController* ctrl = bs_adapter_attach_reload_batch_create(4);
    BS_TEST_REQUIRE("ctrl", ctrl != nullptr);
    bs_adapter_attach_reload_batch_set_read_fn(ctrl, facade_read_fn, fix);
    bs_adapter_attach_reload_batch_set_default_gate(ctrl);
    bs_adapter_attach_reload_batch_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_PATH);
    bs_adapter_attach_reload_batch_set_attach_ctx(ctrl, fix->ctx);
    bs_adapter_attach_reload_batch_set_manifest_path(ctrl, manifest.string().c_str());
    BS_TEST_REQUIRE("add", bs_adapter_attach_reload_batch_add_path(ctrl, uri.c_str()) == 0);
    const int rc = bs_adapter_attach_reload_batch_run(ctrl);
    const int ok =
        (rc == 0 && bs_adapter_attach_reload_batch_outcome(ctrl) == BATCH_ALL_OK) ? 0 : -1;
    bs_adapter_attach_reload_batch_destroy(ctrl);
    return ok;
}

static int run_reload_with_sync_failure(BsTestAttachIoFixture* fix, const fs::path& manifest,
                                        const std::string& uri)
{
    ReloadBatchController* ctrl = bs_adapter_attach_reload_batch_create(4);
    BS_TEST_REQUIRE("ctrl-fail", ctrl != nullptr);
    bs_adapter_attach_reload_batch_set_read_fn(ctrl, facade_read_fn, fix);
    bs_adapter_attach_reload_batch_set_default_gate(ctrl);
    bs_adapter_attach_reload_batch_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_PATH);
    bs_adapter_attach_reload_batch_set_attach_ctx(ctrl, fix->ctx);
    bs_adapter_attach_reload_batch_set_manifest_path(ctrl, manifest.string().c_str());
    BS_TEST_REQUIRE("add-fail", bs_adapter_attach_reload_batch_add_path(ctrl, uri.c_str()) == 0);
    bs_adapter_attach_config_testing_set_sync_fail_path(uri.c_str());
    const int rc = bs_adapter_attach_reload_batch_run(ctrl);
    bs_adapter_attach_config_testing_clear_sync_fail_path();
    const int ok = (rc == 0 && bs_adapter_attach_reload_batch_outcome(ctrl) ==
                                 BATCH_COMPLETED_WITH_FAILURES)
                       ? 0
                       : -1;
    bs_adapter_attach_reload_batch_destroy(ctrl);
    return ok;
}

static int bootstrap_fixture(BsTestAttachIoFixture* fix)
{
    fix->ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("ctx", fix->ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(fix) == 0);
    BS_TEST_REQUIRE("io", bs_test_attach_open_io(fix) == 0);
    bs_adapter_attach_ctx_set_active(fix->ctx);
    return 0;
}

static int test_manifest_revision_authority(const fs::path& work)
{
    BsTestAttachIoFixture fix{};
    BS_TEST_REQUIRE("bootstrap-fixture", bootstrap_fixture(&fix) == 0);

    const fs::path cfg      = work / "dual_truth.json";
    const fs::path manifest = work / "dual_truth.manifest";
    BS_TEST_REQUIRE("write", bs_test_write_binary_file(cfg, kBlessStarConfigV1Golden,
                                                       kBlessStarConfigV1GoldenLen));
    BS_TEST_REQUIRE("ctx-store",
                    bs_adapter_attach_ctx_open_persist_store(fix.ctx,
                                                             manifest.string().c_str()) == 0);

    const std::string uri = bs_test_path_to_file_uri(cfg);
    BS_TEST_REQUIRE("reload-1", run_reload(&fix, manifest, uri) == 0);

    BsAttachStore* store = bs_adapter_attach_ctx_persist_store(fix.ctx);
    uint64_t       manifest_rev = 0;
    BS_TEST_REQUIRE("manifest-rev",
                    bs_adapter_attach_persist_store_get_revision(store, uri.c_str(),
                                                                 &manifest_rev) == BS_ATTACH_OK);
    BsAttachSnapshotMeta meta{};
    BS_TEST_REQUIRE("meta", bs_adapter_attach_config_get_snapshot_meta(fix.ctx, uri.c_str(),
                                                                       &meta) == 0);
    BS_TEST_REQUIRE("rev-aligned", meta.revision == manifest_rev);
    BS_TEST_REQUIRE("kernel-reset",
                    bs_adapter_attach_kernel_testing_count_non_idle_stages(fix.ctx) == 0u);

    int      handle = -1;
    uint64_t handle_rev = 0;
    BS_TEST_REQUIRE("open-handle",
                    bs_adapter_attach_config_open_snapshot_read(fix.ctx, uri.c_str(), &handle,
                                                               &handle_rev) == 0);

    BsAttachStore* external = bs_adapter_attach_persist_store_open(manifest.string().c_str());
    BS_TEST_REQUIRE("external-store", external != nullptr);
    BS_TEST_REQUIRE("external-commit",
                    bs_adapter_attach_persist_store_commit_per_path(
                        external, uri.c_str(), kBlessStarConfigV1Golden, kBlessStarConfigV1GoldenLen,
                        manifest_rev) == BS_ATTACH_OK);
    bs_adapter_attach_persist_store_close(external);

    BS_TEST_REQUIRE("stale-meta",
                    bs_adapter_attach_config_get_snapshot_meta(fix.ctx, uri.c_str(), &meta) ==
                        BS_ATTACH_ERR_REVISION_STALE);

    unsigned char chunk[64];
    size_t        out_len = 0;
    BS_TEST_REQUIRE("stale-chunk",
                    bs_adapter_attach_config_read_snapshot_chunk(fix.ctx, handle, 0, chunk,
                                                                sizeof(chunk), &out_len) ==
                        BS_ATTACH_ERR_REVISION_STALE);

    BS_TEST_REQUIRE("reload-2", run_reload(&fix, manifest, uri) == 0);
    BS_TEST_REQUIRE("old-handle-invalid",
                    bs_adapter_attach_config_read_snapshot_chunk(fix.ctx, handle, 0, chunk,
                                                                sizeof(chunk), &out_len) ==
                        BS_ATTACH_CONC_ERR_REVISION_CHANGED);
    bs_adapter_attach_config_close_snapshot_read(fix.ctx, handle);

    BS_TEST_REQUIRE("meta-after-reload",
                    bs_adapter_attach_config_get_snapshot_meta(fix.ctx, uri.c_str(), &meta) == 0);
    BS_TEST_REQUIRE("manifest-after-reload",
                    bs_adapter_attach_persist_store_get_revision(store, uri.c_str(),
                                                                 &manifest_rev) == BS_ATTACH_OK);
    BS_TEST_REQUIRE("rev-realigned", meta.revision == manifest_rev);
    bs_test_attach_teardown(&fix);
    return 0;
}

static int test_sync_failure_rejects_batch(const fs::path& work)
{
    BsTestAttachIoFixture fix{};
    BS_TEST_REQUIRE("bootstrap-fixture-fail", bootstrap_fixture(&fix) == 0);

    const fs::path cfg      = work / "skip_sync.json";
    const fs::path manifest = work / "skip_sync.manifest";
    BS_TEST_REQUIRE("write-fail", bs_test_write_binary_file(cfg, kBlessStarConfigV1Golden,
                                                            kBlessStarConfigV1GoldenLen));
    BS_TEST_REQUIRE("ctx-store-fail",
                    bs_adapter_attach_ctx_open_persist_store(fix.ctx,
                                                             manifest.string().c_str()) == 0);
    const std::string uri = bs_test_path_to_file_uri(cfg);
    BS_TEST_REQUIRE("sync-fail", run_reload_with_sync_failure(&fix, manifest, uri) == 0);
    bs_test_attach_teardown(&fix);
    return 0;
}

int main()
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_attach_dual_truth"));
    BS_TEST_REQUIRE("dual-truth", test_manifest_revision_authority(tmp_guard.path) == 0);
    BS_TEST_REQUIRE("sync-reject", test_sync_failure_rejects_batch(tmp_guard.path) == 0);
    std::fprintf(stderr, "AttachDualTruthSyncTest: PASS\n");
    return 0;
}
