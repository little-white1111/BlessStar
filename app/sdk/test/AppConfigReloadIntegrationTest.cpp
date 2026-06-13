/**
 * P-7: ConfigReloadSession integration tests
 *
 * End-to-end flow: vendor file -> normalize -> session(AddPath with v1 bytes) -> Commit -> report
 */

#include "bs/kernel/report/report.h"

#include "bs/adapter/attach_context.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/orchestration/reload_with_report.h"
#include "bs/adapter/orchestration/reload_gate_default.h"
#include "bs/adapter/parser/config_parse.h"

#include "bs/app/sdk/config_reload_session.h"
#include "bs/app/sdk/app_scenario_policy.h"
#include "bs/app/sdk/app_vendor_precheck.h"
#include "bs/app/sdk/vendor_config_normalizer.h"
#include "bs/app/sdk/vendor_reload_facade.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <string>

#include "support/attach_test_fixture.h"
#include "support/day12_attach_fixture.h"
#include "support/test_temp_dir.h"

namespace fs = std::filesystem;

// ============================================================================
// 1. Session with in-memory v1 bytes + default gate
// ============================================================================

static int test_session_in_memory_v1_default_gate()
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_session_v1_default"));
    const fs::path&          temp_dir = tmp_guard.path;

    BsTestAttachIoFixture fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("setup", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);

    {
        const fs::path vendor_file =
            fs::absolute("app/sdk/test/fixtures/vendor_generic_business_good.json");
        BS_TEST_REQUIRE("fixture", fs::exists(vendor_file));

        bs::app::NormalizeResult norm;
        BS_TEST_REQUIRE("normalize", bs::app::NormalizeVendorConfig(
                                         bs::app::VendorFormat::GenericBusinessJson,
                                         vendor_file.string(), &norm) &&
                                         norm.ok);

        bs::app::ConfigReloadSession session(fix.ctx);

        const uint8_t* raw = reinterpret_cast<const uint8_t*>(norm.v1_bytes.data());
        BS_TEST_REQUIRE("add-v1", session.AddPath("main", raw, norm.v1_bytes.size()));

        Report* report = session.Commit();
        BS_TEST_REQUIRE("commit", report != nullptr);

        char* json = bs_report_to_json(report);
        if (json)
        {
            std::fprintf(stdout, "report: %s\n", json);
            std::free(json);
        }

        ReportStatus st = bs_report_get_status(report);
        std::fprintf(stdout, "session commit status: %d\n", static_cast<int>(st));

        Report* taken = session.TakeReport();
        BS_TEST_REQUIRE("taken", taken == report);
        if (taken)
            bs_report_destroy(taken);
    }

    bs_test_attach_teardown(&fix);
    return 0;
}

// ============================================================================
// 2. Session with policy gate + in-memory v1 bytes
// ============================================================================

static int test_session_with_policy_gate()
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_session_policy"));
    const fs::path&          temp_dir = tmp_guard.path;

    BsTestAttachIoFixture fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("setup", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);

    {
        const fs::path vendor_file =
            fs::absolute("app/sdk/test/fixtures/vendor_generic_business_good.json");
        BS_TEST_REQUIRE("fixture", fs::exists(vendor_file));

        bs::app::NormalizeResult norm;
        BS_TEST_REQUIRE("normalize", bs::app::NormalizeVendorConfig(
                                         bs::app::VendorFormat::GenericBusinessJson,
                                         vendor_file.string(), &norm) &&
                                         norm.ok);

        bs::app::ConfigReloadSession session(fix.ctx);

        bs::app::ScenarioPolicy policy;
        policy.type             = bs::app::ScenarioType::ExpenseReimburse;
        policy.tenant           = "tenant-a";
        policy.allow_hot_reload = true;
        policy.max_batch        = 64;
        session.AddPolicyGate(policy);

        const uint8_t* raw = reinterpret_cast<const uint8_t*>(norm.v1_bytes.data());
        BS_TEST_REQUIRE("add-v1", session.AddPath("main", raw, norm.v1_bytes.size()));

        Report* report = session.Commit();
        BS_TEST_REQUIRE("commit", report != nullptr);

        std::fprintf(stdout, "policy gate session status: %d\n",
                     static_cast<int>(bs_report_get_status(report)));

        Report* taken = session.TakeReport();
        if (taken)
            bs_report_destroy(taken);
    }

    bs_test_attach_teardown(&fix);
    return 0;
}

// ============================================================================
// 3. Session with SetNoGate
// ============================================================================

static int test_session_no_gate()
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_session_no_gate"));
    const fs::path&          temp_dir = tmp_guard.path;

    BsTestAttachIoFixture fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("setup", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);

    {
        bs::app::ConfigReloadSession session(fix.ctx);
        session.SetNoGate();

        const uint8_t raw[] = {0xFF, 0xFE, 0xFD};
        BS_TEST_REQUIRE("add-raw", session.AddPath("raw", raw, 3));

        Report* report = session.Commit();
        BS_TEST_REQUIRE("commit", report != nullptr);

        std::fprintf(stdout, "no-gate session status: %d\n",
                     static_cast<int>(bs_report_get_status(report)));

        Report* taken = session.TakeReport();
        if (taken)
            bs_report_destroy(taken);
    }

    bs_test_attach_teardown(&fix);
    return 0;
}

// ============================================================================
// 4. Session reuse: Commit -> Reset -> Commit again
// ============================================================================

static int test_session_reuse()
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_session_reuse"));
    const fs::path&          temp_dir = tmp_guard.path;

    BsTestAttachIoFixture fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("setup", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);

    {
        bs::app::ConfigReloadSession session(fix.ctx);

        // First cycle
        {
            const uint8_t data[] = {1, 2, 3};
            BS_TEST_REQUIRE("cycle1-add", session.AddPath("c1", data, 3));
            Report* r = session.Commit();
            BS_TEST_REQUIRE("cycle1-commit", r != nullptr);
            Report* taken = session.TakeReport();
            if (taken)
                bs_report_destroy(taken);
        }

        session.Reset();

        // Second cycle
        {
            const uint8_t data[] = {4, 5, 6};
            BS_TEST_REQUIRE("cycle2-add", session.AddPath("c2", data, 3));
            Report* r = session.Commit();
            BS_TEST_REQUIRE("cycle2-commit", r != nullptr);
            Report* taken = session.TakeReport();
            if (taken)
                bs_report_destroy(taken);
        }
    }

    bs_test_attach_teardown(&fix);
    return 0;
}

// ============================================================================
// 5. Session with custom gate
// ============================================================================

static int test_session_custom_gate()
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_session_custom"));
    const fs::path&          temp_dir = tmp_guard.path;

    BsTestAttachIoFixture fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("setup", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);

    {
        const fs::path vendor_file =
            fs::absolute("app/sdk/test/fixtures/vendor_generic_business_good.json");
        BS_TEST_REQUIRE("fixture", fs::exists(vendor_file));

        bs::app::NormalizeResult norm;
        BS_TEST_REQUIRE("normalize", bs::app::NormalizeVendorConfig(
                                         bs::app::VendorFormat::GenericBusinessJson,
                                         vendor_file.string(), &norm) &&
                                         norm.ok);

        struct CustomCtx
        {
            bool ran = false;
        } custom_ctx;

        auto custom_fn = [](const void* data, size_t len, char* err, size_t err_cap,
                            void* ctx) -> int {
            auto* cctx = static_cast<CustomCtx*>(ctx);
            cctx->ran  = true;
            if (!data || len == 0)
            {
                std::snprintf(err, err_cap, "empty data in custom gate");
                return -1;
            }
            return 0;
        };

        bs::app::ConfigReloadSession session(fix.ctx);
        session.AddCustomGate(custom_fn, &custom_ctx);

        const uint8_t* raw = reinterpret_cast<const uint8_t*>(norm.v1_bytes.data());
        BS_TEST_REQUIRE("add-v1", session.AddPath("main", raw, norm.v1_bytes.size()));

        Report* report = session.Commit();
        BS_TEST_REQUIRE("commit", report != nullptr);

        BS_TEST_REQUIRE("custom-ran", custom_ctx.ran);

        Report* taken = session.TakeReport();
        if (taken)
            bs_report_destroy(taken);
    }

    bs_test_attach_teardown(&fix);
    return 0;
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    int rc = 0;

    rc |= test_session_in_memory_v1_default_gate();
    rc |= test_session_with_policy_gate();
    rc |= test_session_no_gate();
    rc |= test_session_reuse();
    rc |= test_session_custom_gate();

    if (rc != 0)
        std::fprintf(stderr, "FAIL: some integration tests returned non-zero\n");
    return rc;
}
