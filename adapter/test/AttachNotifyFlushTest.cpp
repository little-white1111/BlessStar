/**
 * REC-G03-4: weak notify consistency with write-window flush on outer close.
 */

#include "bs/adapter/attach_config.h"
#include "bs/adapter/attach_session.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"

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

static void event_listener(const ConfigEvent*, void* user_data)
{
    auto* hits = static_cast<int*>(user_data);
    ++(*hits);
}

static void watch_listener(const char*, ConfigEventType, const void*, void* user_data)
{
    auto* hits = static_cast<int*>(user_data);
    ++(*hits);
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

int main()
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_attach_notify_flush"));
    const fs::path           work = tmp_guard.path;

    BsTestAttachIoFixture fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("ctx", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("io", bs_test_attach_open_io(&fix) == 0);
    bs_adapter_attach_ctx_set_active(fix.ctx);

    const fs::path cfg      = work / "notify.json";
    const fs::path manifest = work / "notify.manifest";
    BS_TEST_REQUIRE("write", bs_test_write_binary_file(cfg, kBlessStarConfigV1Golden,
                                                       kBlessStarConfigV1GoldenLen));
    BS_TEST_REQUIRE("ctx-store",
                    bs_adapter_attach_ctx_open_persist_store(fix.ctx,
                                                             manifest.string().c_str()) == 0);
    const std::string uri = bs_test_path_to_file_uri(cfg);

    int event_hits = 0;
    int watch_hits = 0;
    EventBus* bus = bs_adapter_attach_config_event_bus(fix.ctx);
    BS_TEST_REQUIRE("bus", bus != nullptr);
    BS_TEST_REQUIRE("event-sub", bs_event_bus_subscribe(bus, uri.c_str(), event_listener,
                                                        &event_hits) == 0);
    BS_TEST_REQUIRE("watch-sub",
                    bs_adapter_attach_config_subscribe_state_watch(
                        fix.ctx, uri.c_str(), watch_listener, &watch_hits) == 0);

    bs_adapter_attach_session_begin_write_window(fix.ctx);
    BS_TEST_REQUIRE("sync-window",
                    bs_adapter_attach_config_sync_path(fix.ctx, uri.c_str(),
                                                       kBlessStarConfigV1Golden,
                                                       kBlessStarConfigV1GoldenLen) == 0);
    BS_TEST_REQUIRE("event-deferred", event_hits == 0);
    bs_adapter_attach_session_end_write_window(fix.ctx);
    BS_TEST_REQUIRE("event-flushed", event_hits > 0);

    event_hits = 0;
    watch_hits = 0;
    BS_TEST_REQUIRE("reload", run_reload(&fix, manifest, uri) == 0);
    BS_TEST_REQUIRE("event-after-reload", event_hits > 0);
    BS_TEST_REQUIRE("watch-after-reload", watch_hits > 0);

    bs_test_attach_teardown(&fix);
    std::fprintf(stderr, "AttachNotifyFlushTest: PASS\n");
    return 0;
}
