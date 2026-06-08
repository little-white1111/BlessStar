/**
 * Day 22 / REC-A'-5 + AG-REC-CONC-1: concurrent reads during RECOVERING must reject.
 * @arch-gap REC-O-01
 *
 * Repro: Step-1 leaves ctx in RECOVERING; parallel get_snapshot_meta must not observe
 *        stale runtime truth (REC-A'-5).
 * Before fix: theoretical dirty read if recovering guard missing.
 * After fix:  all concurrent readers return BS_ATTACH_ERR_RECOVERING until Step-2 READY.
 */

#include "bs/kernel/report/report.h"

#include "bs/adapter/attach_config.h"
#include "bs/adapter/attach_errors.h"
#include "bs/adapter/attach_recover.h"
#include "bs/adapter/attach_session.h"
#include "bs/adapter/persistence/attach_store.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
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

static int write_config(const fs::path& path)
{
    return bs_test_write_binary_file(path, kBlessStarConfigV1Golden, kBlessStarConfigV1GoldenLen)
               ? 0
               : -1;
}

static int prime_manifest(const fs::path& config_path, const fs::path& manifest_path,
                          std::string* uri_out)
{
    const std::string uri = bs_test_path_to_file_uri(config_path);
    BsAttachStore*    store = bs_adapter_attach_persist_store_open(manifest_path.string().c_str());
    BS_TEST_REQUIRE("prime-store", store != nullptr);
    BS_TEST_REQUIRE("prime-commit",
                    bs_adapter_attach_persist_store_commit_per_path(
                        store, uri.c_str(), kBlessStarConfigV1Golden, kBlessStarConfigV1GoldenLen,
                        0) == BS_ATTACH_OK);
    bs_adapter_attach_persist_store_close(store);
    *uri_out = uri;
    return 0;
}

static int test_concurrent_reads_during_recovering(const fs::path& manifest_path,
                                                   const std::string& uri)
{
    AttachContext* ctx =
        bs_adapter_attach_recover_from_store(manifest_path.string().c_str(), nullptr);
    BS_TEST_REQUIRE("step1", ctx != nullptr);
    BS_TEST_REQUIRE("recovering", bs_adapter_attach_session_is_recovering(ctx) == 1);

    constexpr int kReaders = 12;
    constexpr int kIters   = 80;

    std::atomic<int> wrong_rc{0};
    std::atomic<int> ok_reads{0};

    auto reader = [&]()
    {
        for (int i = 0; i < kIters; ++i)
        {
            BsAttachSnapshotMeta meta{};
            const int rc = bs_adapter_attach_config_get_snapshot_meta(ctx, uri.c_str(), &meta);
            if (rc == BS_ATTACH_ERR_RECOVERING)
                continue;
            if (rc == 0)
                ok_reads.fetch_add(1);
            else
                wrong_rc.fetch_add(1);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(kReaders);
    for (int i = 0; i < kReaders; ++i)
        threads.emplace_back(reader);
    for (auto& t : threads)
        t.join();

    BS_TEST_REQUIRE("no-ok-read", ok_reads.load() == 0);
    BS_TEST_REQUIRE("no-wrong-rc", wrong_rc.load() == 0);

    bs_adapter_attach_ctx_destroy(ctx);
    return 0;
}

static std::atomic<int> g_slow_read_ms{40};

static int slow_facade_read(void* user_ctx, const char* uri, IoReadResult* out)
{
    const int rc = facade_read_fn(user_ctx, uri, out);
    std::this_thread::sleep_for(std::chrono::milliseconds(g_slow_read_ms.load()));
    return rc;
}

static int test_concurrent_reads_during_cold_reload(const fs::path& manifest_path,
                                                    const std::string& uri)
{
    AttachContext* ctx =
        bs_adapter_attach_recover_from_store(manifest_path.string().c_str(), nullptr);
    BS_TEST_REQUIRE("step1", ctx != nullptr);

    BsTestAttachIoFixture fix{};
    fix.ctx = ctx;
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("io", bs_test_attach_open_io(&fix) == 0);
    bs_adapter_attach_ctx_set_active(fix.ctx);

    Report* report = bs_report_create("recover_conc");
    BS_TEST_REQUIRE("report", report != nullptr);

    BsAttachRecoverColdReloadOptions opts{};
    opts.struct_size = sizeof(opts);
    const std::string manifest_str = manifest_path.string();
    opts.manifest_path             = manifest_str.c_str();
    opts.read_fn                   = slow_facade_read;
    opts.read_ctx                  = &fix;
    opts.scheme                    = BS_ATTACH_SCHEME_PER_PATH;
    opts.max_inflight              = 4;
    opts.report                    = report;

    std::atomic<int> reload_rc{BS_ATTACH_ERR_IO};
    std::atomic<int> wrong_rc{0};
    std::atomic<int> ok_reads{0};

    std::thread reload_thread(
        [&]()
        {
            reload_rc.store(bs_adapter_attach_recover_cold_reload(ctx, &opts));
        });

    std::vector<std::thread> readers;
    for (int i = 0; i < 8; ++i)
    {
        readers.emplace_back(
            [&]()
            {
                for (int j = 0; j < 60; ++j)
                {
                    BsAttachSnapshotMeta meta{};
                    const int rc =
                        bs_adapter_attach_config_get_snapshot_meta(ctx, uri.c_str(), &meta);
                    if (rc == BS_ATTACH_ERR_RECOVERING)
                        continue;
                    if (rc == 0)
                        ok_reads.fetch_add(1);
                    else
                        wrong_rc.fetch_add(1);
                }
            });
    }

    reload_thread.join();
    for (auto& t : readers)
        t.join();

    BS_TEST_REQUIRE("reload-ok", reload_rc.load() == 0);
    BS_TEST_REQUIRE("ready", bs_adapter_attach_session_is_recovering(ctx) == 0);
    BS_TEST_REQUIRE("no-ok-during-reload", ok_reads.load() == 0);
    BS_TEST_REQUIRE("no-wrong-rc", wrong_rc.load() == 0);

    bs_report_destroy(report);
    bs_test_attach_teardown(&fix);
    return 0;
}

int main()
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_attach_recover_conc"));
    const fs::path           work     = tmp_guard.path;
    const fs::path           cfg      = work / "recover.json";
    const fs::path           manifest = work / "manifest.bs";

    BS_TEST_REQUIRE("write", write_config(cfg) == 0);

    std::string uri;
    BS_TEST_REQUIRE("prime", prime_manifest(cfg, manifest, &uri) == 0);
    BS_TEST_REQUIRE("conc-recovering", test_concurrent_reads_during_recovering(manifest, uri) == 0);
    BS_TEST_REQUIRE("conc-reload", test_concurrent_reads_during_cold_reload(manifest, uri) == 0);

    std::fprintf(stderr, "AttachRecoverConcurrencyTest: PASS\n");
    return 0;
}
