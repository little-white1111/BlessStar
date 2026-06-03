/**
 * M3 / IMPL-06-03: file:// v1 JSON -> IoFacade read -> default gate (parse + ir_gate) -> COMMITTED.
 *
 * Uses an isolated temp dir per run (C-ST-10) so parallel regression does not share manifest paths.
 */

#include "bs/kernel/report/report.h"

#include "bs/adapter/orchestration/reload_with_report.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <string>

#include "support/attach_test_fixture.h"
#include "support/config_v1_golden.h"
#include "support/day12_attach_fixture.h"
#include "support/test_temp_dir.h"

namespace fs = std::filesystem;

static int facade_read_fn(void* user_ctx, const char* uri, IoReadResult* out)
{
    auto* fix = static_cast<BsTestAttachIoFixture*>(user_ctx);
    return bs_io_facade_read(fix->io, uri, out);
}

int main()
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_reload_config_json"));
    const fs::path&          work = tmp_guard.path;

    BsTestAttachIoFixture fix{};
    fix.ctx = bs_attach_context_create();
    BS_TEST_REQUIRE("setup", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);

    const fs::path good_file = work / "v1_good.json";
    BS_TEST_REQUIRE("write-good", bs_test_write_binary_file(good_file, kBlessStarConfigV1Golden,
                                                            kBlessStarConfigV1GoldenLen));
    const std::string good_uri = bs_test_path_to_file_uri(good_file);

    ReloadBatchController* ctrl = bs_reload_batch_controller_create(8);
    BS_TEST_REQUIRE("reload", ctrl != nullptr);
    bs_reload_batch_controller_set_read_fn(ctrl, facade_read_fn, &fix);
    const fs::path manifest_path = work / "manifest.bs";
    bs_reload_batch_controller_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_PATH);
    bs_reload_batch_controller_set_manifest_path(ctrl, manifest_path.string().c_str());

    Report* report = report_create("reload_config_json_m3");
    BS_TEST_REQUIRE("report", report != nullptr);

    BS_TEST_REQUIRE("good-add", bs_reload_batch_add_path(ctrl, good_uri.c_str()) == 0);
    BS_TEST_REQUIRE("good-run", bs_reload_batch_run_with_report(ctrl, report) == 0);
    BS_TEST_REQUIRE("good-outcome", bs_reload_batch_outcome(ctrl) == BATCH_ALL_OK);

    char* json = report_to_json(report);
    BS_TEST_REQUIRE("good-json", json != nullptr);
    BS_TEST_REQUIRE("good-report", std::strstr(json, "parse") == nullptr);
    BS_TEST_REQUIRE("good-report", std::strstr(json, "ir_gate") == nullptr);
    std::free(json);

    const fs::path bad_file = work / "v1_bad.json";
    BS_TEST_REQUIRE("write-bad", bs_test_write_binary_file(bad_file, "{ not valid json", 18));
    const std::string bad_uri = bs_test_path_to_file_uri(bad_file);

    BS_TEST_REQUIRE("bad-add", bs_reload_batch_add_path(ctrl, bad_uri.c_str()) == 0);
    BS_TEST_REQUIRE("bad-run", bs_reload_batch_run_with_report(ctrl, report) == 0);
    BS_TEST_REQUIRE("bad-outcome", bs_reload_batch_outcome(ctrl) == BATCH_COMPLETED_WITH_FAILURES);

    json = report_to_json(report);
    BS_TEST_REQUIRE("bad-json", json != nullptr);
    BS_TEST_REQUIRE("bad-report", std::strstr(json, "parse") != nullptr);
    std::free(json);

    report_destroy(report);
    bs_reload_batch_controller_destroy(ctrl);
    bs_test_attach_teardown(&fix);
    return 0;
}
