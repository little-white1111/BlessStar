/**
 * Day 22 / REC-A': explicit two-step attach crash recovery.
 */

#include "bs/kernel/report/report.h"

#include "bs/adapter/attach_errors.h"
#include "bs/adapter/attach_recover.h"
#include "bs/adapter/attach_session.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/persistence/attach_store.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <string>

#if !defined(_WIN32)
#include <signal.h>
#include <unistd.h>

#include <sys/wait.h>
#endif

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

static int write_config(const fs::path& path)
{
    return bs_test_write_binary_file(path, kBlessStarConfigV1Golden, kBlessStarConfigV1GoldenLen)
               ? 0
               : -1;
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
    bs_adapter_attach_persist_store_close(store);
    *uri_out = uri;
    return 0;
}

static int assert_snapshot_equals(AttachContext* ctx, const std::string& uri)
{
    unsigned char buf[4096];
    size_t        out_size = 0;
    uint64_t      rev      = 0;
    // clang-format off
    const int     copy_rc  =
        bs_adapter_attach_config_get_snapshot_copy(ctx, uri.c_str(), buf, sizeof(buf), &out_size,
                                                   &rev);
    // clang-format on
    if (copy_rc != 0)
        std::fprintf(stderr, "snapshot-copy rc=%d uri=%s recovering=%d\n", copy_rc, uri.c_str(),
                     bs_adapter_attach_session_is_recovering(ctx));
    BS_TEST_REQUIRE("snapshot-copy", copy_rc == 0);
    BS_TEST_REQUIRE("snapshot-size", out_size == kBlessStarConfigV1GoldenLen);
    BS_TEST_REQUIRE("snapshot-rev", rev >= 1);
    BS_TEST_REQUIRE("snapshot-bytes",
                    std::memcmp(buf, kBlessStarConfigV1Golden, kBlessStarConfigV1GoldenLen) == 0);
    return 0;
}

static int recover_with_io(const fs::path& manifest_path, const std::string& uri)
{
    AttachContext* ctx =
        bs_adapter_attach_recover_from_store(manifest_path.string().c_str(), nullptr);
    BS_TEST_REQUIRE("recover-step1", ctx != nullptr);
    BS_TEST_REQUIRE("recovering-flag", bs_adapter_attach_session_is_recovering(ctx) == 1);

    BsAttachSnapshotMeta meta{};
    BS_TEST_REQUIRE("recovering-read", bs_adapter_attach_config_get_snapshot_meta(
                                           ctx, uri.c_str(), &meta) == BS_ATTACH_ERR_RECOVERING);

    BsTestAttachIoFixture fix{};
    fix.ctx = ctx;
    BS_TEST_REQUIRE("recover-bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("recover-freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("recover-io", bs_test_attach_open_io(&fix) == 0);
    bs_adapter_attach_ctx_set_active(fix.ctx);

    Report* report = bs_report_create("recover_cold");
    BS_TEST_REQUIRE("recover-report", report != nullptr);

    BsAttachRecoverColdReloadOptions opts{};
    opts.struct_size               = sizeof(opts);
    const std::string manifest_str = manifest_path.string();
    opts.manifest_path             = manifest_str.c_str();
    opts.io_facade                 = fix.io;
    opts.scheme                    = BS_ATTACH_SCHEME_PER_PATH;
    opts.max_inflight              = 4;
    opts.report                    = report;

    const int recover_rc = bs_adapter_attach_recover_cold_reload(ctx, &opts);
    if (recover_rc != 0)
    {
        char* json = bs_report_to_json(report);
        std::fprintf(stderr, "recover-step2 rc=%d report=%s\n", recover_rc, json ? json : "<null>");
        std::free(json);
    }
    BS_TEST_REQUIRE("recover-step2", recover_rc == 0);
    BS_TEST_REQUIRE("ready-flag", bs_adapter_attach_session_is_recovering(ctx) == 0);
    BS_TEST_REQUIRE("snapshot", assert_snapshot_equals(ctx, uri) == 0);

    bs_report_destroy(report);
    bs_test_attach_teardown(&fix);
    return 0;
}

static int test_failed_cold_reload_keeps_recovering(const fs::path&    manifest_path,
                                                    const std::string& uri)
{
    AttachContext* ctx =
        bs_adapter_attach_recover_from_store(manifest_path.string().c_str(), nullptr);
    BS_TEST_REQUIRE("fail-step1", ctx != nullptr);
    bs_adapter_attach_ctx_set_log_bus_bound(ctx, 1);

    BsAttachRecoverColdReloadOptions opts{};
    opts.struct_size               = sizeof(opts);
    const std::string manifest_str = manifest_path.string();
    opts.manifest_path             = manifest_str.c_str();
    opts.read_fn                   = fail_read_fn;
    opts.scheme                    = BS_ATTACH_SCHEME_PER_PATH;
    opts.max_inflight              = 4;

    BS_TEST_REQUIRE("fail-step2", bs_adapter_attach_recover_cold_reload(ctx, &opts) != 0);
    BS_TEST_REQUIRE("fail-still-recovering", bs_adapter_attach_session_is_recovering(ctx) == 1);

    BsAttachSnapshotMeta meta{};
    BS_TEST_REQUIRE("fail-read-blocked", bs_adapter_attach_config_get_snapshot_meta(
                                             ctx, uri.c_str(), &meta) == BS_ATTACH_ERR_RECOVERING);
    bs_adapter_attach_ctx_destroy(ctx);
    return 0;
}

static int prime_manifest_via_crashed_child(const fs::path& config_path,
                                            const fs::path& manifest_path, std::string* uri_out)
{
#if defined(_WIN32)
    return prime_manifest(config_path, manifest_path, uri_out);
#else
    const pid_t pid = fork();
    if (pid == 0)
    {
        std::string child_uri;
        if (prime_manifest(config_path, manifest_path, &child_uri) != 0)
            _exit(2);
        raise(SIGKILL);
        _exit(3);
    }
    if (pid < 0)
        return 1;
    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
        return 1;
    BS_TEST_REQUIRE("child-sigkill", WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL);
    *uri_out = bs_test_path_to_file_uri(config_path);
    return 0;
#endif
}

int main()
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_attach_recover_cold"));
    const fs::path           work     = tmp_guard.path;
    const fs::path           cfg      = work / "recover.json";
    const fs::path           manifest = work / "manifest.bs";

    BS_TEST_REQUIRE("write", write_config(cfg) == 0);

    std::string uri;
    BS_TEST_REQUIRE("prime-crash", prime_manifest_via_crashed_child(cfg, manifest, &uri) == 0);
    BS_TEST_REQUIRE("recover-success", recover_with_io(manifest, uri) == 0);
    BS_TEST_REQUIRE("recover-fail", test_failed_cold_reload_keeps_recovering(manifest, uri) == 0);

    std::fprintf(stderr, "AttachRecoverColdTest: PASS\n");
    return 0;
}
