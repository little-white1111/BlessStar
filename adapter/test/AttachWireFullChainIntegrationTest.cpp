/**
 * REC-G-03 / WIRE-07-01: full reload chain consumes IrSnapshot entries.
 */

#include "bs/adapter/attach_config.h"
#include "bs/adapter/attach_ir_snapshot.h"
#include "bs/adapter/attach_session.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/persistence/attach_store.h"

#include <cstdio>

#include <filesystem>
#include <string>

#include "support/attach_test_fixture.h"
#include "support/config_v1_golden.h"
#include "support/test_temp_dir.h"

namespace fs = std::filesystem;

static int facade_read_fn(void* user_ctx, const char* uri, IoReadResult* out)
{
    auto* fix = static_cast<BsTestAttachIoFixture*>(user_ctx);
    return bs_io_facade_read(fix->io, uri, out);
}

int main()
{
    std::fprintf(stderr, "trace: begin\n");
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_attach_wire_full"));
    const fs::path           work = tmp_guard.path;

    BsTestAttachIoFixture fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("ctx", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("io", bs_test_attach_open_io(&fix) == 0);
    BS_TEST_REQUIRE("pool-warmed", bs_adapter_attach_ctx_is_kernel_pool_warmed(fix.ctx) == 1);
    bs_adapter_attach_ctx_set_active(fix.ctx);
    std::fprintf(stderr, "trace: bootstrapped\n");

    const fs::path manifest = work / "wire.manifest";
    BS_TEST_REQUIRE("ctx-store", bs_adapter_attach_ctx_open_persist_store(
                                     fix.ctx, manifest.string().c_str()) == 0);

    const fs::path cfg_a = work / "wire_a.json";
    const fs::path cfg_b = work / "wire_b.json";
    BS_TEST_REQUIRE("write-a", bs_test_write_binary_file(cfg_a, kBlessStarConfigV1Golden,
                                                         kBlessStarConfigV1GoldenLen));
    BS_TEST_REQUIRE("write-b", bs_test_write_binary_file(cfg_b, kBlessStarConfigV1Golden,
                                                         kBlessStarConfigV1GoldenLen));
    const std::string uri_a = bs_test_path_to_file_uri(cfg_a);
    const std::string uri_b = bs_test_path_to_file_uri(cfg_b);

    ReloadBatchController* ctrl = bs_adapter_attach_reload_batch_create(4);
    BS_TEST_REQUIRE("ctrl", ctrl != nullptr);
    bs_adapter_attach_reload_batch_set_read_fn(ctrl, facade_read_fn, &fix);
    bs_adapter_attach_reload_batch_set_default_gate(ctrl);
    bs_adapter_attach_reload_batch_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_BATCH);
    bs_adapter_attach_reload_batch_set_attach_ctx(ctrl, fix.ctx);
    bs_adapter_attach_reload_batch_set_manifest_path(ctrl, manifest.string().c_str());
    BS_TEST_REQUIRE("add-a", bs_adapter_attach_reload_batch_add_path(ctrl, uri_a.c_str()) == 0);
    BS_TEST_REQUIRE("add-b", bs_adapter_attach_reload_batch_add_path(ctrl, uri_b.c_str()) == 0);
    std::fprintf(stderr, "trace: before run\n");
    BS_TEST_REQUIRE("run", bs_adapter_attach_reload_batch_run(ctrl) == 0);
    std::fprintf(stderr, "trace: after run\n");
    BS_TEST_REQUIRE("outcome", bs_adapter_attach_reload_batch_outcome(ctrl) == BATCH_ALL_OK);
    BS_TEST_REQUIRE("ir-empty", bs_adapter_attach_ir_snapshot_entry_count(fix.ctx) == 0u);

    BsAttachStore* store = bs_adapter_attach_ctx_persist_store(fix.ctx);
    for (const std::string& uri : {uri_a, uri_b})
    {
        uint64_t manifest_rev = 0;
        BS_TEST_REQUIRE("manifest-rev", bs_adapter_attach_persist_store_get_revision(
                                            store, uri.c_str(), &manifest_rev) == BS_ATTACH_OK);
        BsAttachSnapshotMeta meta{};
        BS_TEST_REQUIRE(
            "meta", bs_adapter_attach_config_get_snapshot_meta(fix.ctx, uri.c_str(), &meta) == 0);
        BS_TEST_REQUIRE("revision", meta.revision == manifest_rev);
    }

    std::fprintf(stderr, "trace: before ctrl destroy\n");
    bs_adapter_attach_reload_batch_destroy(ctrl);
    std::fprintf(stderr, "trace: before fixture teardown\n");
    bs_test_attach_teardown(&fix);
    std::fprintf(stderr, "trace: after fixture teardown\n");
    std::fprintf(stderr, "AttachWireFullChainIntegrationTest: PASS\n");
    return 0;
}
