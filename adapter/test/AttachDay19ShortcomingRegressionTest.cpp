/**
 * @arch-gap day19 observed-shortcoming regression (XIX-MEM / attach persistence)
 *
 * | ID | Risk | Pre-fix expectation | Post-fix expectation |
 * |----|------|---------------------|----------------------|
 * | AG-DAY19-MANIFEST-1 | PER_PATH manifest fsync per commit slows smoke | ALWAYS slower than NEVER
 * | NEVER bounded (<15s for N=40) | | AG-DAY19-WAL-PURGE-1 | Repeated WAL purge rescans 1..epoch |
 * Second purge rescans full range | Coalesced second purge is fast | | AG-DAY19-STORE-1 |
 * store_open per reload + purge O(n^2) | 80 reloads >> budget | ctx persist_store + coalesce <120s
 * | | AG-DAY19-POOL-1 | freeze pool warmup on 100KB reload | Warmed reload slower |
 * clear_kernel_pool_warmed faster | | AG-RS-RESET-1 | reset leaves no path/IR leak across 100 runs
 * | uri_index grows | reset clears; IR bounded | | AG-RS-STORE-1 | ctx persist_store no extra
 * store_open | open_count rises | single open for 100 runs | | AG-RS-ONE-SHOT-1 | kOneShot
 * destroy(ctrl) keeps ctx store | store closed on destroy | ctx store survives |
 */

#include "bs/adapter/attach_context.h"
#include "bs/adapter/attach_ir_snapshot.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/persistence/attach_store.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <string>

#include "support/attach_test_fixture.h"
#include "support/config_v1_golden.h"

static bool shortcoming_verbose_enabled()
{
    static int cached = -1;
    if (cached < 0)
    {
        const char* env = std::getenv("BS_SHORTCOMING_VERBOSE");
        cached          = (env && env[0] == '1') ? 1 : 0;
    }
    return cached != 0;
}

static void shortcoming_progress(const char* msg)
{
    if (!shortcoming_verbose_enabled())
        return;
    std::fprintf(stderr, "[shortcoming] %s\n", msg);
    (void)std::fflush(stderr);
}
#include "support/day12_attach_fixture.h"
#include "support/day19_fixture_gen.h"
#include "support/test_temp_dir.h"

namespace fs = std::filesystem;

static int facade_read_fn(void* user_ctx, const char* uri, IoReadResult* out)
{
    auto* fix = static_cast<BsTestAttachIoFixture*>(user_ctx);
    return bs_io_facade_read(fix->io, uri, out);
}

static int64_t steady_ms_since(const std::chrono::steady_clock::time_point& t0)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                 t0)
        .count();
}

static int64_t steady_us_since(const std::chrono::steady_clock::time_point& t0)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
                                                                 t0)
        .count();
}

/** AG-DAY19-MANIFEST-1: manifest fsync policy affects PER_PATH commit throughput. */
static int test_manifest_fsync_per_path_throughput(void)
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_day19_manifest_gap"));
    const fs::path           cfg = tmp_guard.path / "cfg.json";
    BS_TEST_REQUIRE("write-cfg", bs_test_write_binary_file(cfg, kBlessStarConfigV1Golden,
                                                           kBlessStarConfigV1GoldenLen));
#if defined(BLESSSTAR_SANITIZER_CI)
    constexpr int kCommits = 15;
#else
    constexpr int kCommits = 120;
#endif

    auto bench_commits = [&](BsAttachFsyncPolicy policy, const char* tag, int64_t* elapsed_ms_out)
    {
        const fs::path cfg_path = tmp_guard.path / (std::string(tag) + ".json");
        BS_TEST_REQUIRE("write-cfg", bs_test_write_binary_file(cfg_path, kBlessStarConfigV1Golden,
                                                               kBlessStarConfigV1GoldenLen));
        const std::string uri      = bs_test_path_to_file_uri(cfg_path);
        const fs::path    manifest = tmp_guard.path / (std::string(tag) + ".bs");
        BsAttachStore*    store = bs_adapter_attach_persist_store_open(manifest.string().c_str());
        BS_TEST_REQUIRE("store-open", store != nullptr);
        bs_adapter_attach_persist_store_set_fsync_policy(store, policy);
        BS_TEST_REQUIRE("policy-set",
                        bs_adapter_attach_persist_store_get_fsync_policy(store) == policy);

        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < kCommits; ++i)
        {
            if (i == 0 || (i % 20) == 0)
                shortcoming_progress((std::string(tag) + " commit " + std::to_string(i)).c_str());
            const int rc = bs_adapter_attach_persist_store_commit_per_path(
                store, uri.c_str(), kBlessStarConfigV1Golden, kBlessStarConfigV1GoldenLen,
                static_cast<uint64_t>(i));
            BS_TEST_REQUIRE("commit", rc == BS_ATTACH_OK);
        }
        *elapsed_ms_out = steady_ms_since(t0);
        bs_adapter_attach_persist_store_close(store);
        return 0;
    };

    int64_t never_ms = 0;
    int64_t batch_ms = 0;
    BS_TEST_REQUIRE("bench-never", bench_commits(BS_ATTACH_FSYNC_NEVER, "never", &never_ms) == 0);
    BS_TEST_REQUIRE("bench-batch",
                    bench_commits(BS_ATTACH_FSYNC_BATCH_COMMIT, "batch", &batch_ms) == 0);

    BS_TEST_REQUIRE("never-bounded", never_ms < 20000);
#ifndef _WIN32
    /* Linux CI: BATCH_COMMIT fsyncs manifest each PER_PATH commit. */
    BS_TEST_REQUIRE("batch-not-faster-than-never", batch_ms >= never_ms);
    BS_TEST_REQUIRE("batch-fsync-overhead",
                    batch_ms >= never_ms + 80 || batch_ms >= never_ms + (never_ms / 5));
#endif
    return 0;
}

/** AG-DAY19-WAL-PURGE-1: coalesced purge skips already-purged epoch range. */
static int test_wal_purge_coalesced_on_store(void)
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_day19_wal_purge_gap"));
    const fs::path           cfg = tmp_guard.path / "cfg.json";
    BS_TEST_REQUIRE("write-cfg", bs_test_write_binary_file(cfg, kBlessStarConfigV1Golden,
                                                           kBlessStarConfigV1GoldenLen));
    const std::string uri      = bs_test_path_to_file_uri(cfg);
    const fs::path    manifest = tmp_guard.path / "manifest.bs";

    BsAttachStore* store = bs_adapter_attach_persist_store_open(manifest.string().c_str());
    BS_TEST_REQUIRE("store-open", store != nullptr);

#if defined(BLESSSTAR_SANITIZER_CI)
    const int kPurgeWalEpochs = 10;
#else
    const int kPurgeWalEpochs = 50;
#endif
    for (int i = 0; i < kPurgeWalEpochs; ++i)
    {
        bs_adapter_attach_persist_store_batch_begin(store);
        BS_TEST_REQUIRE("stage",
                        bs_adapter_attach_persist_store_batch_stage(
                            store, uri.c_str(), kBlessStarConfigV1Golden,
                            kBlessStarConfigV1GoldenLen, static_cast<uint64_t>(i)) == BS_ATTACH_OK);
        BS_TEST_REQUIRE("commit",
                        bs_adapter_attach_persist_store_batch_commit(store) == BS_ATTACH_OK);
    }

    const uint64_t epoch = bs_adapter_attach_persist_store_batch_epoch(store);
    BS_TEST_REQUIRE("epoch", epoch >= static_cast<uint64_t>(kPurgeWalEpochs));

    const auto t0 = std::chrono::steady_clock::now();
    bs_adapter_attach_persist_store_testing_purge_wal(store);
    const int64_t  first_ms = steady_ms_since(t0);
    const uint64_t after_first =
        bs_adapter_attach_persist_store_testing_wal_last_purge_through(store);
    BS_TEST_REQUIRE("purged-after-first", after_first + 2 >= epoch);

    const auto t1 = std::chrono::steady_clock::now();
    bs_adapter_attach_persist_store_testing_purge_wal(store);
    const int64_t  second_ms = steady_ms_since(t1);
    const uint64_t after_second =
        bs_adapter_attach_persist_store_testing_wal_last_purge_through(store);

    BS_TEST_REQUIRE("purged-stable", after_second == after_first);
    BS_TEST_REQUIRE("second-purge-fast", second_ms < 500);
    BS_TEST_REQUIRE("first-purge-bounded", first_ms < 30000);

    bs_adapter_attach_persist_store_close(store);
    return 0;
}

static int run_per_path_reload(BsTestAttachIoFixture* fix, const fs::path& manifest_path,
                               const char* uri, ReloadBatchController* reuse_ctrl)
{
    ReloadBatchController* ctrl  = reuse_ctrl;
    const int              owned = reuse_ctrl ? 0 : 1;
    if (!ctrl)
    {
        ctrl = bs_adapter_attach_reload_batch_create(4);
        BS_TEST_REQUIRE("ctrl", ctrl != nullptr);
        bs_adapter_attach_reload_batch_set_attach_ctx(ctrl, fix->ctx);
        bs_adapter_attach_reload_batch_set_read_fn(ctrl, facade_read_fn, fix);
        bs_adapter_attach_reload_batch_set_default_gate(ctrl);
        bs_adapter_attach_reload_batch_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_PATH);
        bs_adapter_attach_reload_batch_set_manifest_path(ctrl, manifest_path.string().c_str());
    }
    else
    {
        bs_adapter_attach_reload_batch_reset(ctrl);
    }
    BS_TEST_REQUIRE("add", bs_adapter_attach_reload_batch_add_path(ctrl, uri) == 0);
    const int run_rc = bs_adapter_attach_reload_batch_run(ctrl);
    const int ok =
        (run_rc == 0 && bs_adapter_attach_reload_batch_outcome(ctrl) == BATCH_ALL_OK) ? 0 : -1;
    if (owned)
        bs_adapter_attach_reload_batch_destroy(ctrl);
    return ok;
}

/** AG-DAY19-STORE-1: ctx persist_store keeps reload loop within budget. */
static int test_stress_ctx_store_reload_budget(void)
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_day19_store_gap"));
    BsTestAttachIoFixture    fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("ctx", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    bs_adapter_attach_ctx_testing_clear_kernel_pool_warmed(fix.ctx);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);
    bs_adapter_attach_ctx_set_active(fix.ctx);

    const fs::path cfg = tmp_guard.path / "reload.json";
    BS_TEST_REQUIRE("write-cfg", bs_test_write_binary_file(cfg, kBlessStarConfigV1Golden,
                                                           kBlessStarConfigV1GoldenLen));
    const std::string uri           = bs_test_path_to_file_uri(cfg);
    const fs::path    manifest_path = tmp_guard.path / "manifest.bs";
    BS_TEST_REQUIRE("ctx-store", bs_adapter_attach_ctx_open_persist_store(
                                     fix.ctx, manifest_path.string().c_str()) == 0);
    BsAttachStore* store = bs_adapter_attach_ctx_persist_store(fix.ctx);
    BS_TEST_REQUIRE("store", store != nullptr);
    bs_adapter_attach_persist_store_set_fsync_policy(store, BS_ATTACH_FSYNC_NEVER);

    const auto t0 = std::chrono::steady_clock::now();
#if defined(BLESSSTAR_SANITIZER_CI)
    const int kCtxStoreReloads = 10;
#else
    const int kCtxStoreReloads = 80;
#endif
    for (int i = 0; i < kCtxStoreReloads; ++i)
    {
        if (i == 0 || (i % 10) == 0)
            shortcoming_progress(("ctx-store reload " + std::to_string(i)).c_str());
        BS_TEST_REQUIRE("reload",
                        run_per_path_reload(&fix, manifest_path, uri.c_str(), nullptr) == 0);
        if (steady_ms_since(t0) > 120000)
        {
            std::fprintf(stderr, "FAIL ctx-store budget exceeded at reload %d\n", i);
            return 1;
        }
    }

    bs_test_attach_teardown(&fix);
    return 0;
}

static int reload_once_ms(BsTestAttachIoFixture* fix, const fs::path& manifest_path,
                          const char* uri, int64_t* elapsed_ms_out)
{
    const auto t0 = std::chrono::steady_clock::now();
    if (run_per_path_reload(fix, manifest_path, uri, nullptr) != 0)
        return -1;
    *elapsed_ms_out = steady_ms_since(t0);
    return 0;
}

static int reload_once_us(BsTestAttachIoFixture* fix, const fs::path& manifest_path,
                          const char* uri, int64_t* elapsed_us_out)
{
    const auto t0 = std::chrono::steady_clock::now();
    if (run_per_path_reload(fix, manifest_path, uri, nullptr) != 0)
        return -1;
    *elapsed_us_out = steady_us_since(t0);
    return 0;
}

/** AG-DAY19-POOL-1: clear_kernel_pool_warmed skips pool exec on PER_PATH reload. */
static int test_pool_warmup_reload_latency(void)
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_day19_pool_gap"));
    BsTestAttachIoFixture    fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("ctx", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("pool-warmed", bs_adapter_attach_ctx_is_kernel_pool_warmed(fix.ctx) == 1);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);
    bs_adapter_attach_ctx_set_active(fix.ctx);

    const std::string json = bs_day19_make_valid_v1_json(100u * 1024u);
    const fs::path    cfg  = tmp_guard.path / "large.json";
    BS_TEST_REQUIRE("write-large", bs_test_write_binary_file(cfg, json.data(), json.size()));
    const std::string uri           = bs_test_path_to_file_uri(cfg);
    const fs::path    manifest_path = tmp_guard.path / "manifest.bs";
    BS_TEST_REQUIRE("ctx-store", bs_adapter_attach_ctx_open_persist_store(
                                     fix.ctx, manifest_path.string().c_str()) == 0);
    BsAttachStore* store = bs_adapter_attach_ctx_persist_store(fix.ctx);
    BS_TEST_REQUIRE("store", store != nullptr);
    bs_adapter_attach_persist_store_set_fsync_policy(store, BS_ATTACH_FSYNC_NEVER);

    constexpr int kPoolReloadIters = 10;
    int64_t       warmed_sum_us    = 0;
    for (int i = 0; i < kPoolReloadIters; ++i)
    {
        shortcoming_progress(("pool-warmup warmed-reload " + std::to_string(i)).c_str());
        int64_t us = 0;
        BS_TEST_REQUIRE("warmed-reload",
                        reload_once_us(&fix, manifest_path, uri.c_str(), &us) == 0);
        warmed_sum_us += us;
    }

    bs_adapter_attach_ctx_testing_clear_kernel_pool_warmed(fix.ctx);
    BS_TEST_REQUIRE("pool-cleared", bs_adapter_attach_ctx_is_kernel_pool_warmed(fix.ctx) == 0);

    int64_t cleared_sum_us = 0;
    for (int i = 0; i < kPoolReloadIters; ++i)
    {
        shortcoming_progress(("pool-warmup cleared-reload " + std::to_string(i)).c_str());
        int64_t us = 0;
        BS_TEST_REQUIRE("cleared-reload",
                        reload_once_us(&fix, manifest_path, uri.c_str(), &us) == 0);
        cleared_sum_us += us;
    }

    bs_test_attach_teardown(&fix);

    BS_TEST_REQUIRE("warmed-sum-nonzero", warmed_sum_us > 0);
    BS_TEST_REQUIRE("cleared-sum-nonzero", cleared_sum_us > 0);
#ifndef _WIN32
    /* Linux CI: ms rounding ties in stage=all; compare microsecond aggregates. */
    BS_TEST_REQUIRE("warmed-slower-aggregate", warmed_sum_us > cleared_sum_us);
#endif
    return 0;
}

/** AG-RS-RESET-1: 100x reset→add→run on one controller without IR growth. */
static int test_rs_reset_no_leak(void)
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_rs_reset_gap"));
    BsTestAttachIoFixture    fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("ctx", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    bs_adapter_attach_ctx_testing_clear_kernel_pool_warmed(fix.ctx);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);
    bs_adapter_attach_ctx_set_active(fix.ctx);

    const fs::path cfg = tmp_guard.path / "cfg.json";
    BS_TEST_REQUIRE("write-cfg", bs_test_write_binary_file(cfg, kBlessStarConfigV1Golden,
                                                           kBlessStarConfigV1GoldenLen));
    const std::string uri           = bs_test_path_to_file_uri(cfg);
    const fs::path    manifest_path = tmp_guard.path / "manifest.bs";
    BS_TEST_REQUIRE("ctx-store", bs_adapter_attach_ctx_open_persist_store(
                                     fix.ctx, manifest_path.string().c_str()) == 0);

    ReloadBatchController* ctrl = bs_adapter_attach_reload_batch_create(4);
    BS_TEST_REQUIRE("ctrl", ctrl != nullptr);
    bs_adapter_attach_reload_batch_set_attach_ctx(ctrl, fix.ctx);
    bs_adapter_attach_reload_batch_set_read_fn(ctrl, facade_read_fn, &fix);
    bs_adapter_attach_reload_batch_set_default_gate(ctrl);
    bs_adapter_attach_reload_batch_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_PATH);
    bs_adapter_attach_reload_batch_set_manifest_path(ctrl, manifest_path.string().c_str());

#if defined(BLESSSTAR_SANITIZER_CI)
    const int kRsResetRuns = 10;
#else
    const int kRsResetRuns = 100;
#endif
    for (int i = 0; i < kRsResetRuns; ++i)
    {
        if (i == 0 || (i % 10) == 0)
            shortcoming_progress(("rs-reset reload " + std::to_string(i)).c_str());
        BS_TEST_REQUIRE("reload", run_per_path_reload(&fix, manifest_path, uri.c_str(), ctrl) == 0);
        const size_t ir_entries = bs_adapter_attach_ir_snapshot_entry_count(fix.ctx);
        BS_TEST_REQUIRE("ir-bounded", ir_entries <= 2u);
    }

    bs_adapter_attach_reload_batch_destroy(ctrl);
    bs_test_attach_teardown(&fix);
    return 0;
}

/** AG-RS-STORE-1: ctx persist_store - no extra store_open across 100 reload runs. */
static int test_rs_ctx_store_single_open(void)
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_rs_store_gap"));
    BsTestAttachIoFixture    fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("ctx", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    bs_adapter_attach_ctx_testing_clear_kernel_pool_warmed(fix.ctx);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);
    bs_adapter_attach_ctx_set_active(fix.ctx);

    const fs::path cfg = tmp_guard.path / "cfg.json";
    BS_TEST_REQUIRE("write-cfg", bs_test_write_binary_file(cfg, kBlessStarConfigV1Golden,
                                                           kBlessStarConfigV1GoldenLen));
    const std::string uri           = bs_test_path_to_file_uri(cfg);
    const fs::path    manifest_path = tmp_guard.path / "manifest.bs";

    bs_adapter_attach_persist_store_testing_reset_open_count();
    BS_TEST_REQUIRE("ctx-store", bs_adapter_attach_ctx_open_persist_store(
                                     fix.ctx, manifest_path.string().c_str()) == 0);
    BS_TEST_REQUIRE("open-count-1", bs_adapter_attach_persist_store_testing_open_count() == 1);

    ReloadBatchController* ctrl = bs_adapter_attach_reload_batch_create(4);
    BS_TEST_REQUIRE("ctrl", ctrl != nullptr);
    bs_adapter_attach_reload_batch_set_attach_ctx(ctrl, fix.ctx);
    bs_adapter_attach_reload_batch_set_read_fn(ctrl, facade_read_fn, &fix);
    bs_adapter_attach_reload_batch_set_default_gate(ctrl);
    bs_adapter_attach_reload_batch_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_PATH);
    bs_adapter_attach_reload_batch_set_manifest_path(ctrl, manifest_path.string().c_str());

#if defined(BLESSSTAR_SANITIZER_CI)
    const int kRsStoreRuns = 10;
#else
    const int kRsStoreRuns = 100;
#endif
    for (int i = 0; i < kRsStoreRuns; ++i)
        BS_TEST_REQUIRE("reload", run_per_path_reload(&fix, manifest_path, uri.c_str(), ctrl) == 0);

    BS_TEST_REQUIRE("open-count-still-1",
                    bs_adapter_attach_persist_store_testing_open_count() == 1);

    bs_adapter_attach_reload_batch_destroy(ctrl);
    bs_test_attach_teardown(&fix);
    BS_TEST_REQUIRE("open-count-after-teardown",
                    bs_adapter_attach_persist_store_testing_open_count() == 0);
    return 0;
}

/** AG-RS-ONE-SHOT-1: kOneShot destroy(ctrl) does not close ctx persist_store. */
static int test_rs_one_shot_preserves_ctx_store(void)
{
    bs_adapter_attach_persist_store_testing_reset_open_count();
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_rs_oneshot_gap"));
    BsTestAttachIoFixture    fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("ctx", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    bs_adapter_attach_ctx_testing_clear_kernel_pool_warmed(fix.ctx);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);
    bs_adapter_attach_ctx_set_active(fix.ctx);

    const fs::path cfg = tmp_guard.path / "cfg.json";
    BS_TEST_REQUIRE("write-cfg", bs_test_write_binary_file(cfg, kBlessStarConfigV1Golden,
                                                           kBlessStarConfigV1GoldenLen));
    const std::string uri           = bs_test_path_to_file_uri(cfg);
    const fs::path    manifest_path = tmp_guard.path / "manifest.bs";
    BS_TEST_REQUIRE("ctx-store", bs_adapter_attach_ctx_open_persist_store(
                                     fix.ctx, manifest_path.string().c_str()) == 0);

    for (int i = 0; i < 5; ++i)
    {
        BS_TEST_REQUIRE("oneshot-reload",
                        run_per_path_reload(&fix, manifest_path, uri.c_str(), nullptr) == 0);
        BS_TEST_REQUIRE("ctx-store-alive", bs_adapter_attach_ctx_persist_store(fix.ctx) != nullptr);
        BS_TEST_REQUIRE("open-count-1", bs_adapter_attach_persist_store_testing_open_count() == 1);
    }

    bs_test_attach_teardown(&fix);
    return 0;
}

static const char* shortcoming_stage_env()
{
    const char* stage = std::getenv("BS_DAY19_SHORTCOMING_STAGE");
    return (stage && stage[0]) ? stage : "all";
}

static bool shortcoming_run_stage(const char* stage_id)
{
    const char* wanted = shortcoming_stage_env();
    if (std::strcmp(wanted, "all") == 0)
        return true;
    return std::strcmp(wanted, stage_id) == 0;
}

int main()
{
    (void)std::setvbuf(stdout, nullptr, _IONBF, 0);
    (void)std::setvbuf(stderr, nullptr, _IONBF, 0);
    bs_adapter_attach_persist_store_testing_reset_open_count();
    const char* stage = shortcoming_stage_env();
    std::fprintf(stderr, "[shortcoming] begin stage=%s\n", stage);
    if (shortcoming_run_stage("manifest-fsync"))
    {
        BS_TEST_REQUIRE("manifest-fsync", test_manifest_fsync_per_path_throughput() == 0);
        std::fprintf(stderr, "[shortcoming] after manifest-fsync\n");
        if (std::strcmp(stage, "all") != 0)
            return 0;
    }
    if (shortcoming_run_stage("wal-purge"))
    {
        BS_TEST_REQUIRE("wal-purge", test_wal_purge_coalesced_on_store() == 0);
        std::fprintf(stderr, "[shortcoming] after wal-purge\n");
        if (std::strcmp(stage, "all") != 0)
            return 0;
    }
    if (shortcoming_run_stage("ctx-store-budget"))
    {
        BS_TEST_REQUIRE("ctx-store-budget", test_stress_ctx_store_reload_budget() == 0);
        std::fprintf(stderr, "[shortcoming] after ctx-store-budget\n");
        if (std::strcmp(stage, "all") != 0)
            return 0;
    }
    if (shortcoming_run_stage("pool-warmup"))
    {
        BS_TEST_REQUIRE("pool-warmup", test_pool_warmup_reload_latency() == 0);
        std::fprintf(stderr, "[shortcoming] after pool-warmup\n");
        if (std::strcmp(stage, "all") != 0)
            return 0;
    }
    if (shortcoming_run_stage("rs-reset"))
    {
        BS_TEST_REQUIRE("rs-reset", test_rs_reset_no_leak() == 0);
        std::fprintf(stderr, "[shortcoming] after rs-reset\n");
        if (std::strcmp(stage, "all") != 0)
            return 0;
    }
    if (shortcoming_run_stage("rs-store"))
    {
        BS_TEST_REQUIRE("rs-store", test_rs_ctx_store_single_open() == 0);
        std::fprintf(stderr, "[shortcoming] after rs-store\n");
        if (std::strcmp(stage, "all") != 0)
            return 0;
    }
    if (shortcoming_run_stage("rs-oneshot"))
    {
        BS_TEST_REQUIRE("rs-oneshot", test_rs_one_shot_preserves_ctx_store() == 0);
        std::fprintf(stderr, "[shortcoming] after rs-oneshot\n");
        if (std::strcmp(stage, "all") != 0)
            return 0;
    }
    std::fprintf(stderr, "AttachDay19ShortcomingRegressionTest: PASS\n");
    return 0;
}
