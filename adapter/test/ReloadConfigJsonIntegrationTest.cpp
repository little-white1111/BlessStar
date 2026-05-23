/**
 * M3 / IMPL-06-03: file:// v1 JSON -> IoFacade read -> default gate (parse + ir_gate) -> COMMITTED.
 */

#include "bs/kernel/report/report.h"

#include "bs/adapter/orchestration/reload_with_report.h"

#include <cstdio>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <string>

#include "support/attach_test_fixture.h"
#include "support/config_v1_golden.h"
#include "support/day12_attach_fixture.h"

namespace fs = std::filesystem;

static int facade_read_fn(void* user_ctx, const char* uri, IoReadResult* out)
{
    auto* fix = static_cast<BsTestAttachIoFixture*>(user_ctx);
    return bs_io_facade_read(fix->io, uri, out);
}

int main()
{
    BsTestAttachIoFixture fix{};
    fix.ctx = bs_attach_context_create();
    if (!fix.ctx)
        return 1;
    if (bs_test_attach_bootstrap_begin_ctx(&fix) != 0)
        return 1;
    if (bs_test_attach_bootstrap_freeze_ctx(&fix) != 0)
        return 1;
    if (bs_test_attach_open_io(&fix) != 0)
        return 1;

    const fs::path good_file = fs::absolute("bs_reload_config_v1_good.json");
    {
        std::ofstream out(good_file, std::ios::binary);
        out.write(kBlessStarConfigV1Golden,
                  static_cast<std::streamsize>(kBlessStarConfigV1GoldenLen));
    }
    std::string good_uri = "file:///" + good_file.string();
    for (char& c : good_uri)
    {
        if (c == '\\')
            c = '/';
    }

    ReloadBatchController* ctrl = bs_reload_batch_controller_create(8);
    if (!ctrl)
        return 1;
    bs_reload_batch_controller_set_read_fn(ctrl, facade_read_fn, &fix);
    const fs::path manifest_path = good_file.parent_path() / "bs_manifest_day9.json";
    bs_reload_batch_controller_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_PATH);
    bs_reload_batch_controller_set_manifest_path(ctrl, manifest_path.string().c_str());

    Report* report = report_create("reload_config_json_m3");
    if (!report)
        return 1;

    if (bs_reload_batch_add_path(ctrl, good_uri.c_str()) != 0)
        return 1;
    if (bs_reload_batch_run_with_report(ctrl, report) != 0)
        return 1;
    if (bs_reload_batch_outcome(ctrl) != BATCH_ALL_OK)
        return 1;

    char* json = report_to_json(report);
    if (!json)
        return 1;
    if (std::strstr(json, "parse") != nullptr || std::strstr(json, "ir_gate") != nullptr)
    {
        std::fprintf(stderr, "unexpected failure in report for good config\n");
        std::free(json);
        return 1;
    }
    std::free(json);

    const fs::path bad_file = fs::absolute("bs_reload_config_v1_bad.json");
    {
        std::ofstream out(bad_file, std::ios::binary);
        out << "{ not valid json";
    }
    std::string bad_uri = "file:///" + bad_file.string();
    for (char& c : bad_uri)
    {
        if (c == '\\')
            c = '/';
    }

    if (bs_reload_batch_add_path(ctrl, bad_uri.c_str()) != 0)
        return 1;
    if (bs_reload_batch_run_with_report(ctrl, report) != 0)
        return 1;
    if (bs_reload_batch_outcome(ctrl) != BATCH_COMPLETED_WITH_FAILURES)
        return 1;

    json = report_to_json(report);
    if (!json)
        return 1;
    if (std::strstr(json, "parse") == nullptr)
    {
        std::fprintf(stderr, "expected parse stage in report for bad JSON\n");
        std::free(json);
        return 1;
    }
    std::free(json);

    report_destroy(report);
    bs_reload_batch_controller_destroy(ctrl);

    std::error_code ec;
    fs::remove(good_file, ec);
    fs::remove(bad_file, ec);
    bs_test_attach_teardown(&fix);
    return 0;
}
