/**
 * T5.6: vendor sample -> App facade -> v1 temp file -> reload_gate_default path.
 */

#include "bs/kernel/report/report.h"

#include "bs/adapter/orchestration/reload_with_report.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <string>

#include "bs/app/sdk/app_scenario_policy.h"
#include "bs/app/sdk/app_vendor_precheck.h"
#include "bs/app/sdk/vendor_reload_facade.h"
#include "support/attach_test_fixture.h"
#include "support/day12_attach_fixture.h"
#include "support/test_temp_dir.h"

namespace fs = std::filesystem;

static bs::app::ScenarioPolicy g_precheck_policy;

static int facade_read_fn(void* user_ctx, const char* uri, IoReadResult* out)
{
    auto* fix = static_cast<BsTestAttachIoFixture*>(user_ctx);
    return bs_io_facade_read(fix->io, uri, out);
}

static int app_precheck_fn(void* /*user_ctx*/, const char* /*uri*/, const IoReadResult* read_result,
                           BsReloadGateDetail* detail_out)
{
    std::string err;
    if (!bs::app::PrecheckV1BytesForScenario(read_result->data, read_result->length,
                                             g_precheck_policy, &err))
    {
        if (detail_out)
            std::snprintf(detail_out->buf, sizeof(detail_out->buf), "%s", err.c_str());
        return -1;
    }
    return 0;
}

int main()
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_app_vendor_reload"));
    const fs::path&          temp_dir = tmp_guard.path;

    BsTestAttachIoFixture fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("setup", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);

    const fs::path vendor_file =
        fs::absolute("app/sdk/test/fixtures/vendor_generic_business_good.json");
    BS_TEST_REQUIRE("fixture", fs::exists(vendor_file));

    std::string              good_uri;
    bs::app::NormalizeResult norm;
    BS_TEST_REQUIRE("normalize", bs::app::NormalizeFileToTempUri(
                                     bs::app::VendorFormat::GenericBusinessJson,
                                     vendor_file.string(), temp_dir.string(), &good_uri, &norm) &&
                                     norm.ok);

    g_precheck_policy.type             = bs::app::ScenarioType::ExpenseReimburse;
    g_precheck_policy.tenant           = "tenant-a";
    g_precheck_policy.allow_hot_reload = true;
    g_precheck_policy.max_batch        = 64;

    ReloadBatchController* ctrl = bs_adapter_attach_reload_batch_create(8);
    BS_TEST_REQUIRE("reload", ctrl != nullptr);
    bs_adapter_attach_reload_batch_set_read_fn(ctrl, facade_read_fn, &fix);
    bs_adapter_attach_reload_batch_set_gate_fn(ctrl, app_precheck_fn, nullptr);
    const fs::path manifest_path = temp_dir / "manifest.bs";
    bs_adapter_attach_reload_batch_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_PATH);
    bs_adapter_attach_reload_batch_set_manifest_path(ctrl, manifest_path.string().c_str());

    Report* report = bs_report_create("app_vendor_reload_integration");
    BS_TEST_REQUIRE("report", report != nullptr);

    BS_TEST_REQUIRE("add", bs_adapter_attach_reload_batch_add_path(ctrl, good_uri.c_str()) == 0);
    BS_TEST_REQUIRE("run", bs_adapter_attach_reload_batch_run_with_report(ctrl, report) == 0);
    BS_TEST_REQUIRE("outcome", bs_adapter_attach_reload_batch_outcome(ctrl) == BATCH_ALL_OK);

    char* json = bs_report_to_json(report);
    BS_TEST_REQUIRE("json", json != nullptr);
    BS_TEST_REQUIRE("report", std::strstr(json, "parse") == nullptr);
    BS_TEST_REQUIRE("report", std::strstr(json, "ir_gate") == nullptr);
    std::free(json);

    bs_report_destroy(report);
    bs_adapter_attach_reload_batch_destroy(ctrl);

    bs_test_attach_teardown(&fix);
    return 0;
}
