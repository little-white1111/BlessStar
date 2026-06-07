/**
 * @arch-gap P2 observed-shortcoming regression (day20 / attach)
 *
 * | ID | Risk | Pre-fix expectation | Post-fix expectation |
 * |----|------|---------------------|----------------------|
 * | AG-TLS-LOG | Parallel log harness cross-talk | Wrong callback counts | Per-thread isolated
 * counts | | AG-TLS-CTX | Parallel active ctx cross-talk | Same get_active on two threads |
 * Distinct ctx pointers | | AG-RELOAD-CTX | Off-thread run without set_attach_ctx | run fails (-2)
 * | set_attach_ctx then succeeds | | AG-POOL-WW | PER_BATCH pool exec holds write-window | reads
 * BLOCKED during reload | reads succeed during pool phase |
 */

#include "bs/kernel/common/bs_log.h"
#include "bs/kernel/test_support/bs_test_log_bus.h"

#include "bs/adapter/attach_config.h"
#include "bs/adapter/attach_context.h"
#include "bs/adapter/attach_errors.h"
#include "bs/adapter/attach_session.h"
#include "bs/adapter/log/log_bus.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/orchestration/reload_gate_default.h"

#include <chrono>
#include <cstdio>
#include <cstring>

#include <atomic>
#include <filesystem>
#include <string>
#include <thread>

#include "support/attach_test_fixture.h"
#include "support/config_v1_golden.h"
#include "support/day12_attach_fixture.h"
#include "support/test_temp_dir.h"

namespace fs = std::filesystem;

/** C++17 two-worker sync: both workers must arrive before proceeding (main thread excluded). */
struct ArchGapTwoWorkerSync
{
    std::atomic<int> count{0};

    void arrive_and_wait()
    {
        const int n = count.fetch_add(1, std::memory_order_acq_rel) + 1;
        if (n < 2)
        {
            while (count.load(std::memory_order_acquire) < 2)
                std::this_thread::yield();
        }
    }
};

static void noop_log(uint16_t, BsLogLevel, const char*, void*)
{
}

static void count_log(uint16_t, BsLogLevel, const char*, void* user)
{
    auto* n = static_cast<std::atomic<int>*>(user);
    n->fetch_add(1, std::memory_order_relaxed);
}

/** AG-TLS-LOG: bs_test_log_bind_memory_bus must be per-thread isolated. */
static int test_parallel_test_log_bus_isolated(void)
{
    std::atomic<int>     count_a{0};
    std::atomic<int>     count_b{0};
    std::atomic<int>     worker_fail{0};
    ArchGapTwoWorkerSync bind_sync;
    ArchGapTwoWorkerSync emit_sync;

    auto worker =
        [&](std::atomic<int>* counter, void (*on_line)(uint16_t, BsLogLevel, const char*, void*))
    {
        if (bs_test_log_bind_memory_bus(on_line, counter) != 0)
        {
            worker_fail.store(1);
            return;
        }
        bind_sync.arrive_and_wait();
        bs_log_emit(0, BS_LOG_INFO, "arch-gap-tls-log");
        emit_sync.arrive_and_wait();
    };

    std::thread t1(worker, &count_a, count_log);
    std::thread t2(worker, &count_b, count_log);
    t1.join();
    t2.join();

    BS_TEST_REQUIRE("worker-fail", worker_fail.load() == 0);

    BS_TEST_REQUIRE("count-a", count_a.load() == 1);
    BS_TEST_REQUIRE("count-b", count_b.load() == 1);
    return 0;
}

/** AG-TLS-LOG (adapter path): bs_adapter_log_bind_memory_bus must be per-thread isolated. */
static int test_parallel_adapter_log_bus_isolated(void)
{
    AttachContext* ctx_a = bs_adapter_attach_ctx_create();
    AttachContext* ctx_b = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("adapter-ctx-a", ctx_a != nullptr);
    BS_TEST_REQUIRE("adapter-ctx-b", ctx_b != nullptr);

    std::atomic<int>     count_a{0};
    std::atomic<int>     count_b{0};
    std::atomic<int>     worker_fail{0};
    ArchGapTwoWorkerSync bind_sync;
    ArchGapTwoWorkerSync emit_sync;

    auto worker = [&](AttachContext* ctx, std::atomic<int>* counter)
    {
        AttachScope       scope(ctx);
        AttachActiveGuard guard;
        if (bs_adapter_log_bind_memory_bus(count_log, counter) != 0)
        {
            worker_fail.store(1);
            return;
        }
        bind_sync.arrive_and_wait();
        bs_log_emit(0, BS_LOG_INFO, "arch-gap-adapter-tls-log");
        emit_sync.arrive_and_wait();
        bs_adapter_log_shutdown_if_bound();
    };

    std::thread t1(worker, ctx_a, &count_a);
    std::thread t2(worker, ctx_b, &count_b);
    t1.join();
    t2.join();

    BS_TEST_REQUIRE("adapter-worker-fail", worker_fail.load() == 0);
    BS_TEST_REQUIRE("adapter-count-a", count_a.load() == 1);
    BS_TEST_REQUIRE("adapter-count-b", count_b.load() == 1);

    bs_adapter_attach_ctx_destroy(ctx_a);
    bs_adapter_attach_ctx_destroy(ctx_b);
    return 0;
}

/** AG-TLS-CTX: thread-local active ctx stack must not leak across threads. */
static int test_parallel_active_ctx_isolated(void)
{
    AttachContext* ctx_a = bs_adapter_attach_ctx_create();
    AttachContext* ctx_b = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("ctx-a", ctx_a != nullptr);
    BS_TEST_REQUIRE("ctx-b", ctx_b != nullptr);

    std::atomic<AttachContext*> seen_a{nullptr};
    std::atomic<AttachContext*> seen_b{nullptr};
    ArchGapTwoWorkerSync        record_sync;

    auto worker = [&](AttachContext* ctx, std::atomic<AttachContext*>* out)
    {
        AttachScope       scope(ctx);
        AttachActiveGuard guard;
        out->store(bs_adapter_attach_ctx_get_active());
        record_sync.arrive_and_wait();
    };

    std::thread t1(worker, ctx_a, &seen_a);
    std::thread t2(worker, ctx_b, &seen_b);
    t1.join();
    t2.join();

    BS_TEST_REQUIRE("ptr-a", seen_a.load() == ctx_a);
    BS_TEST_REQUIRE("ptr-b", seen_b.load() == ctx_b);
    BS_TEST_REQUIRE("ptr-ne", seen_a.load() != seen_b.load());

    bs_adapter_attach_ctx_destroy(ctx_a);
    bs_adapter_attach_ctx_destroy(ctx_b);
    return 0;
}

static int golden_read(void* /*user_ctx*/, const char* /*uri*/, IoReadResult* out)
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

/** AG-RELOAD-CTX: off-thread reload requires set_attach_ctx, not set_active on caller thread. */
static int test_reload_off_thread_requires_set_attach_ctx(BsTestAttachIoFixture* fix,
                                                          const char*            uri)
{
    ReloadBatchController* ctrl = bs_adapter_attach_reload_batch_create(4);
    BS_TEST_REQUIRE("ctrl", ctrl != nullptr);
    bs_adapter_attach_reload_batch_set_read_fn(ctrl, golden_read, nullptr);
    bs_adapter_attach_reload_batch_set_default_gate(ctrl);
    day12_wire_reload_defaults(ctrl, BS_ATTACH_SCHEME_PER_PATH);
    BS_TEST_REQUIRE("add", bs_adapter_attach_reload_batch_add_path(ctrl, uri) == 0);

    std::atomic<int> worker_rc{-999};
    std::thread      worker([&] { worker_rc.store(bs_adapter_attach_reload_batch_run(ctrl)); });
    worker.join();
    BS_TEST_REQUIRE("off-thread-no-ctx", worker_rc.load() == -2);

    bs_test_attach_bind_reload_ctx(ctrl, fix->ctx, BS_ATTACH_SCHEME_PER_PATH);
    worker_rc.store(-999);
    std::thread worker2([&] { worker_rc.store(bs_adapter_attach_reload_batch_run(ctrl)); });
    worker2.join();
    BS_TEST_REQUIRE("off-thread-with-ctx", worker_rc.load() == 0);
    BS_TEST_REQUIRE("outcome", bs_adapter_attach_reload_batch_outcome(ctrl) == BATCH_ALL_OK);

    bs_adapter_attach_reload_batch_destroy(ctrl);
    return 0;
}

/** AG-POOL-WW: PER_BATCH pool parallel exec must release write-window so readers progress. */
static int test_per_batch_pool_phase_allows_concurrent_read(BsTestAttachIoFixture* fix,
                                                            const char*            uri,
                                                            const fs::path&        work_dir)
{
    static const char k_seed[] = "seed-config";
    BS_TEST_REQUIRE("seed-sync", bs_adapter_attach_config_sync_path(fix->ctx, uri, k_seed,
                                                                    sizeof(k_seed) - 1) == 0);

    std::atomic<bool> reloading{false};
    std::atomic<bool> reader_done{false};
    std::atomic<int>  reads_ok_while_reloading{0};

    const auto reader_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(90);

    std::thread reader(
        [&]
        {
            while (!reader_done.load(std::memory_order_acquire))
            {
                if (std::chrono::steady_clock::now() > reader_deadline)
                    break;
                if (!reloading.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                    continue;
                }
                BsAttachSnapshotMeta meta{};
                if (bs_adapter_attach_config_get_snapshot_meta(fix->ctx, uri, &meta) == 0)
                    reads_ok_while_reloading.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        });

    ReloadBatchController* ctrl = bs_adapter_attach_reload_batch_create(8);
    BS_TEST_REQUIRE("pool-ctrl", ctrl != nullptr);
    bs_test_attach_bind_reload_ctx(ctrl, fix->ctx, BS_ATTACH_SCHEME_PER_BATCH);
    bs_adapter_attach_reload_batch_set_read_fn(ctrl, golden_read, nullptr);
    bs_adapter_attach_reload_batch_set_default_gate(ctrl);

    const std::string pool1  = bs_test_path_to_file_uri(work_dir / "pool_gap1.json");
    const std::string pool2  = bs_test_path_to_file_uri(work_dir / "pool_gap2.json");
    const std::string pool3  = bs_test_path_to_file_uri(work_dir / "pool_gap3.json");
    const char*       uris[] = {pool1.c_str(), pool2.c_str(), pool3.c_str()};
    for (const char* u : uris)
        BS_TEST_REQUIRE("pool-add", bs_adapter_attach_reload_batch_add_path(ctrl, u) == 0);

    reloading.store(true, std::memory_order_release);
    BS_TEST_REQUIRE("pool-run", bs_adapter_attach_reload_batch_run(ctrl) == 0);
    BS_TEST_REQUIRE("pool-outcome", bs_adapter_attach_reload_batch_outcome(ctrl) == BATCH_ALL_OK);
    reloading.store(false, std::memory_order_release);
    reader_done.store(true, std::memory_order_release);
    reader.join();

    BS_TEST_REQUIRE("read-during-pool", reads_ok_while_reloading.load() > 0);

    bs_adapter_attach_reload_batch_destroy(ctrl);
    return 0;
}

int main()
{
    if (bs_adapter_log_bind_memory_bus(noop_log, nullptr) != 0)
        return 1;

    BS_TEST_REQUIRE("tls-test-log", test_parallel_test_log_bus_isolated() == 0);
    BS_TEST_REQUIRE("tls-adapter-log", test_parallel_adapter_log_bus_isolated() == 0);
    BS_TEST_REQUIRE("tls-active-ctx", test_parallel_active_ctx_isolated() == 0);

    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_attach_p2_arch_gap"));
    BsTestAttachIoFixture    fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("ctx", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("pool-warmed", bs_adapter_attach_ctx_is_kernel_pool_warmed(fix.ctx) == 1);
    bs_adapter_attach_ctx_set_active(fix.ctx);

    const fs::path cfg = tmp_guard.path / "reload_ctx.json";
    BS_TEST_REQUIRE("write-uri", bs_test_write_binary_file(cfg, kBlessStarConfigV1Golden,
                                                           kBlessStarConfigV1GoldenLen));

    const std::string read_uri = bs_test_path_to_file_uri(cfg);
    BS_TEST_REQUIRE("reload-ctx",
                    test_reload_off_thread_requires_set_attach_ctx(&fix, read_uri.c_str()) == 0);
    BS_TEST_REQUIRE("pool-ww", test_per_batch_pool_phase_allows_concurrent_read(
                                   &fix, read_uri.c_str(), tmp_guard.path) == 0);

    bs_test_attach_teardown(&fix);
    std::fprintf(stderr, "AttachP2ShortcomingRegressionTest: PASS\n");
    return 0;
}
