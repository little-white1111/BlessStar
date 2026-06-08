/**
 * Day 22 Phase 3 / REC-A'-11..14: runtime.ckpt sidecar fast hydrate (T-REC.11..13).
 */

#include "bs/adapter/attach_errors.h"
#include "bs/adapter/attach_recover.h"
#include "bs/adapter/attach_recover_sidecar.h"
#include "bs/adapter/attach_session.h"
#include "bs/adapter/persistence/attach_store.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <string>

#include "support/attach_test_fixture.h"
#include "support/config_v1_golden.h"
#include "support/test_temp_dir.h"

namespace fs = std::filesystem;

static int fail_read_fn(void*, const char*, IoReadResult* out)
{
    bs_io_read_result_init(out);
    out->status = BS_IO_ERR_PROVIDER;
    return BS_IO_ERR_PROVIDER;
}

static int prime_manifest(const fs::path& config_path, const fs::path& manifest_path,
                          std::string* uri_out, uint64_t expected_rev = 0)
{
    const std::string uri = bs_test_path_to_file_uri(config_path);
    BsAttachStore*    store = bs_adapter_attach_persist_store_open(manifest_path.string().c_str());
    BS_TEST_REQUIRE("prime-store", store != nullptr);
    BS_TEST_REQUIRE("prime-commit",
                    bs_adapter_attach_persist_store_commit_per_path(
                        store, uri.c_str(), kBlessStarConfigV1Golden, kBlessStarConfigV1GoldenLen,
                        expected_rev) == BS_ATTACH_OK);
    bs_adapter_attach_persist_store_close(store);
    *uri_out = uri;
    return 0;
}

static int assert_snapshot_equals(AttachContext* ctx, const std::string& uri)
{
    unsigned char buf[4096];
    size_t        out_size = 0;
    uint64_t      rev      = 0;
    BS_TEST_REQUIRE("snapshot-copy",
                    bs_adapter_attach_config_get_snapshot_copy(ctx, uri.c_str(), buf, sizeof(buf),
                                                               &out_size, &rev) == 0);
    BS_TEST_REQUIRE("snapshot-size", out_size == kBlessStarConfigV1GoldenLen);
    BS_TEST_REQUIRE("snapshot-bytes",
                    std::memcmp(buf, kBlessStarConfigV1Golden, kBlessStarConfigV1GoldenLen) == 0);
    return 0;
}

static int recover_with_read_fn(const fs::path& manifest, const std::string& uri,
                                ReloadPathReadFn read_fn, void* read_ctx)
{
    AttachContext* ctx =
        bs_adapter_attach_recover_from_store(manifest.string().c_str(), nullptr);
    BS_TEST_REQUIRE("step1", ctx != nullptr);

    BsTestAttachIoFixture fix{};
    fix.ctx = ctx;
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    if (read_ctx == &fix)
        BS_TEST_REQUIRE("io", bs_test_attach_open_io(&fix) == 0);
    bs_adapter_attach_ctx_set_active(fix.ctx);

    BsAttachRecoverColdReloadOptions opts{};
    opts.struct_size = sizeof(opts);
    const std::string manifest_str = manifest.string();
    opts.manifest_path             = manifest_str.c_str();
    opts.read_fn                   = read_fn;
    opts.read_ctx                  = read_ctx;
    opts.io_facade                 = fix.io;
    opts.scheme                    = BS_ATTACH_SCHEME_PER_PATH;
    opts.max_inflight              = 4;

    const int rc = bs_adapter_attach_recover_cold_reload(ctx, &opts);
    if (rc != 0)
    {
        bs_test_attach_teardown(&fix);
        return rc;
    }
    BS_TEST_REQUIRE("step2", rc == 0);
    BS_TEST_REQUIRE("ready", bs_adapter_attach_session_is_recovering(ctx) == 0);
    BS_TEST_REQUIRE("snapshot", assert_snapshot_equals(ctx, uri) == 0);

    bs_test_attach_teardown(&fix);
    return 0;
}

static int test_fast_hydrate_skips_io_read(const fs::path& cfg, const fs::path& manifest,
                                           const std::string& uri)
{
    bs_adapter_attach_recover_sidecar_testing_set_enabled(1);
    BS_TEST_REQUIRE("write-ready",
                    bs_adapter_attach_recover_sidecar_write_ready(nullptr, manifest.string().c_str()) ==
                        BS_ATTACH_OK);
    BS_TEST_REQUIRE("can-fast",
                    bs_adapter_attach_recover_sidecar_can_fast_hydrate(manifest.string().c_str()) == 1);
    BS_TEST_REQUIRE("fast-recover", recover_with_read_fn(manifest, uri, fail_read_fn, nullptr) == 0);
    bs_adapter_attach_recover_sidecar_testing_set_enabled(-1);
    return 0;
}

static int test_corrupt_sidecar_falls_back_to_cold(const fs::path& cfg, const fs::path& manifest,
                                                   const std::string& uri)
{
    bs_adapter_attach_recover_sidecar_testing_set_enabled(1);
    BS_TEST_REQUIRE("write-ready",
                    bs_adapter_attach_recover_sidecar_write_ready(nullptr, manifest.string().c_str()) ==
                        BS_ATTACH_OK);

    {
        const std::string sidecar_path = manifest.string() + ".runtime.ckpt";
        std::fstream      f(sidecar_path, std::ios::in | std::ios::out | std::ios::binary);
        BS_TEST_REQUIRE("open-sidecar", f.is_open());
        f.seekp(16);
        f.put(static_cast<char>(0xFF));
    }
    BS_TEST_REQUIRE("cannot-fast",
                    bs_adapter_attach_recover_sidecar_can_fast_hydrate(manifest.string().c_str()) == 0);

    AttachContext* ctx =
        bs_adapter_attach_recover_from_store(manifest.string().c_str(), nullptr);
    BS_TEST_REQUIRE("step1", ctx != nullptr);
    BsTestAttachIoFixture fix{};
    fix.ctx = ctx;
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("io", bs_test_attach_open_io(&fix) == 0);
    bs_adapter_attach_ctx_set_active(fix.ctx);

    BsAttachRecoverColdReloadOptions opts{};
    opts.struct_size = sizeof(opts);
    const std::string manifest_str = manifest.string();
    opts.manifest_path             = manifest_str.c_str();
    opts.io_facade                 = fix.io;
    opts.scheme                    = BS_ATTACH_SCHEME_PER_PATH;
    opts.max_inflight              = 4;

    BS_TEST_REQUIRE("cold-fallback", bs_adapter_attach_recover_cold_reload(ctx, &opts) == 0);
    BS_TEST_REQUIRE("snapshot", assert_snapshot_equals(ctx, uri) == 0);
    bs_test_attach_teardown(&fix);
    bs_adapter_attach_recover_sidecar_testing_set_enabled(-1);
    return 0;
}

static int test_sidecar_disabled_uses_cold(const fs::path& manifest, const std::string& uri)
{
    bs_adapter_attach_recover_sidecar_testing_set_enabled(0);
    BS_TEST_REQUIRE("write-ready",
                    bs_adapter_attach_recover_sidecar_write_ready(nullptr, manifest.string().c_str()) ==
                        BS_ATTACH_OK);
    BS_TEST_REQUIRE("cannot-fast",
                    bs_adapter_attach_recover_sidecar_can_fast_hydrate(manifest.string().c_str()) == 0);
    BS_TEST_REQUIRE("cold-fail", recover_with_read_fn(manifest, uri, fail_read_fn, nullptr) != 0);
    bs_adapter_attach_recover_sidecar_testing_set_enabled(-1);
    return 0;
}

int main()
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_attach_recover_sidecar"));
    const fs::path           work     = tmp_guard.path;
    const fs::path           cfg      = work / "recover_sidecar.json";
    const fs::path           manifest = work / "manifest_sidecar.bs";

    BS_TEST_REQUIRE("write", bs_test_write_binary_file(cfg, kBlessStarConfigV1Golden,
                                                     kBlessStarConfigV1GoldenLen));

    std::string uri;
    BS_TEST_REQUIRE("prime", prime_manifest(cfg, manifest, &uri) == 0);
    BS_TEST_REQUIRE("fast-path", test_fast_hydrate_skips_io_read(cfg, manifest, uri) == 0);

    const fs::path manifest2 = work / "manifest_sidecar2.bs";
    BS_TEST_REQUIRE("prime2", prime_manifest(cfg, manifest2, &uri) == 0);
    BS_TEST_REQUIRE("fallback", test_corrupt_sidecar_falls_back_to_cold(cfg, manifest2, uri) == 0);

    const fs::path manifest3 = work / "manifest_sidecar3.bs";
    BS_TEST_REQUIRE("prime3", prime_manifest(cfg, manifest3, &uri) == 0);
    BS_TEST_REQUIRE("flag-off", test_sidecar_disabled_uses_cold(manifest3, uri) == 0);

    std::fprintf(stderr, "AttachRecoverSidecarTest: PASS\n");
    return 0;
}
