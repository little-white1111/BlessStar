/**
 * T20.9 / XX-CONC: concurrent attach session reads + single-writer reload; reentrancy guard.
 */

#include "bs/kernel/state/EventBus.h"

#include "bs/adapter/attach_config.h"
#include "bs/adapter/attach_errors.h"
#include "bs/adapter/attach_session.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/orchestration/reload_with_report.h"

#include <cstdio>
#include <cstring>

#include <atomic>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "support/attach_test_fixture.h"
#include "support/config_v1_golden.h"
#include "support/day12_attach_fixture.h"
#include "support/test_temp_dir.h"

namespace fs = std::filesystem;

static const char* kLargePath = "/config/conc_large";

static int facade_read_fn(void* user_ctx, const char* uri, IoReadResult* out)
{
    auto* fix = static_cast<BsTestAttachIoFixture*>(user_ctx);
    return bs_io_facade_read(fix->io, uri, out);
}

static int run_one_reload(BsTestAttachIoFixture* fix, const char* uri)
{
    ReloadBatchController* ctrl = bs_adapter_attach_reload_batch_create(4);
    if (!ctrl)
        return -1;
    bs_adapter_attach_reload_batch_set_read_fn(ctrl, facade_read_fn, fix);
    bs_adapter_attach_reload_batch_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_PATH);
    const int add_rc = bs_adapter_attach_reload_batch_add_path(ctrl, uri);
    const int run_rc = (add_rc == 0) ? bs_adapter_attach_reload_batch_run(ctrl) : -1;
    const int ok =
        (run_rc == 0 && bs_adapter_attach_reload_batch_outcome(ctrl) == BATCH_ALL_OK) ? 0 : -1;
    bs_adapter_attach_reload_batch_destroy(ctrl);
    return ok;
}

struct ReloadReentryProbe
{
    AttachContext* ctx;
    const char*    uri;
    int            sync_rc;
};

static void reload_reentry_listener(const ConfigEvent* event, void* user_data)
{
    (void)event;
    auto*             p       = static_cast<ReloadReentryProbe*>(user_data);
    static const char kTiny[] = "{}";
    p->sync_rc = bs_adapter_attach_config_sync_path(p->ctx, p->uri, kTiny, sizeof(kTiny) - 1);
}

static int test_concurrent_read_write(BsTestAttachIoFixture* fix, const std::string& uri)
{
    BS_TEST_REQUIRE("prime", run_one_reload(fix, uri.c_str()) == 0);

    std::atomic<int>      read_errors{0};
    std::atomic<int>      write_done{0};
    std::atomic<uint64_t> last_rev{0};

    auto reader = [&]()
    {
        for (int i = 0; i < 40; ++i)
        {
            BsAttachSnapshotMeta meta{};
            const int rc = bs_adapter_attach_config_get_snapshot_meta(fix->ctx, uri.c_str(), &meta);
            if (rc == BS_ATTACH_CONC_ERR_READ_BLOCKED)
                continue;
            if (rc != 0)
            {
                read_errors.fetch_add(1);
                return;
            }
            const uint64_t prev = last_rev.load(std::memory_order_relaxed);
            if (meta.revision < prev)
                read_errors.fetch_add(1);
            uint64_t observed = last_rev.load(std::memory_order_relaxed);
            while (meta.revision > observed &&
                   !last_rev.compare_exchange_weak(observed, meta.revision))
            {
            }
        }
    };

    std::thread writers[1];
    std::thread readers[10];
    writers[0] = std::thread(
        [&]()
        {
            for (int i = 0; i < 8; ++i)
            {
                if (run_one_reload(fix, uri.c_str()) != 0)
                    read_errors.fetch_add(1);
                write_done.fetch_add(1);
            }
        });
    for (auto& t : readers)
        t = std::thread(reader);
    writers[0].join();
    for (auto& t : readers)
        t.join();

    BS_TEST_REQUIRE("read-errors", read_errors.load() == 0);
    BS_TEST_REQUIRE("writes", write_done.load() == 8);
    return 0;
}

static int test_pending_write_blocks_new_read(BsTestAttachIoFixture* fix, const std::string& uri)
{
    BS_TEST_REQUIRE("prime", run_one_reload(fix, uri.c_str()) == 0);
    bs_adapter_attach_session_begin_write_window(fix->ctx);
    BsAttachSnapshotMeta meta{};
    const int blocked = bs_adapter_attach_config_get_snapshot_meta(fix->ctx, uri.c_str(), &meta);
    bs_adapter_attach_session_end_write_window(fix->ctx);
    BS_TEST_REQUIRE("read-blocked", blocked == BS_ATTACH_CONC_ERR_READ_BLOCKED);
    return 0;
}

static int test_wait_notify_timeout(BsTestAttachIoFixture* fix, const std::string& uri)
{
    const uint64_t future_rev = bs_adapter_attach_session_path_revision(fix->ctx, uri.c_str()) + 99;
    const int      rc = bs_adapter_attach_config_wait_notify(fix->ctx, uri.c_str(), future_rev, 50);
    BS_TEST_REQUIRE("notify-timeout", rc == BS_ATTACH_CONC_ERR_NOTIFY_TIMEOUT);
    return 0;
}

static int test_reload_reentrant_from_listener(BsTestAttachIoFixture* fix, const std::string& uri)
{
    EventBus* bus = bs_adapter_attach_config_event_bus(fix->ctx);
    BS_TEST_REQUIRE("bus", bus != nullptr);

    ReloadReentryProbe probe{fix->ctx, uri.c_str(), 0};
    BS_TEST_REQUIRE("sub",
                    bs_event_bus_subscribe(bus, uri.c_str(), reload_reentry_listener, &probe) == 0);

    BS_TEST_REQUIRE("reload", run_one_reload(fix, uri.c_str()) == 0);
    BS_TEST_REQUIRE("reentrant", probe.sync_rc == BS_ATTACH_CONC_ERR_REENTRANT);
    return 0;
}

static int test_large_snapshot_chunked(BsTestAttachIoFixture* fix)
{
    constexpr size_t           kLarge = 12u * 1024u * 1024u;
    std::vector<unsigned char> payload(kLarge, 0x5a);
    payload[0]          = '{';
    payload[kLarge - 1] = '}';

    BS_TEST_REQUIRE("sync", bs_adapter_attach_config_sync_path(fix->ctx, kLargePath, payload.data(),
                                                               payload.size()) == 0);

    BsAttachSnapshotMeta meta{};
    BS_TEST_REQUIRE("meta",
                    bs_adapter_attach_config_get_snapshot_meta(fix->ctx, kLargePath, &meta) == 0);
    BS_TEST_REQUIRE("meta-size", meta.total_size == kLarge);

    int      handle = -1;
    uint64_t rev    = 0;
    BS_TEST_REQUIRE("open", bs_adapter_attach_config_open_snapshot_read(fix->ctx, kLargePath,
                                                                        &handle, &rev) == 0);

    size_t                     offset     = 0;
    size_t                     total_read = 0;
    std::vector<unsigned char> chunk(meta.chunk_cap ? meta.chunk_cap : 65536);
    while (offset < kLarge)
    {
        size_t    n  = 0;
        const int rc = bs_adapter_attach_config_read_snapshot_chunk(fix->ctx, handle, offset,
                                                                    chunk.data(), chunk.size(), &n);
        BS_TEST_REQUIRE("chunk", rc == 0);
        if (n == 0)
            break;
        total_read += n;
        offset += n;
    }
    BS_TEST_REQUIRE("full", total_read == kLarge);
    bs_adapter_attach_config_close_snapshot_read(fix->ctx, handle);
    return 0;
}

int main()
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_attach_conc"));
    const fs::path           work = tmp_guard.path;

    BsTestAttachIoFixture fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("ctx", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);
    bs_adapter_attach_ctx_set_active(fix.ctx);

    const fs::path cfg = work / "conc.json";
    BS_TEST_REQUIRE("write", bs_test_write_binary_file(cfg, kBlessStarConfigV1Golden,
                                                       kBlessStarConfigV1GoldenLen));
    const std::string uri = bs_test_path_to_file_uri(cfg);

    BS_TEST_REQUIRE("conc-rw", test_concurrent_read_write(&fix, uri) == 0);
    BS_TEST_REQUIRE("pending-write", test_pending_write_blocks_new_read(&fix, uri) == 0);
    BS_TEST_REQUIRE("wait-notify", test_wait_notify_timeout(&fix, uri) == 0);
    BS_TEST_REQUIRE("reentry", test_reload_reentrant_from_listener(&fix, uri) == 0);
    BS_TEST_REQUIRE("chunk", test_large_snapshot_chunked(&fix) == 0);

    bs_test_attach_teardown(&fix);
    std::fprintf(stderr, "AttachConcurrencyTest: PASS\n");
    return 0;
}
