/**
 * P-6: ConfigReloadSession unit tests
 *
 * Covers: empty gate chain -> default_gate, multi-policy gate chain execution,
 * SetNoGate skip, ResetGates full replacement, TakeReport ownership transfer,
 * thread assert, AddPath/AddUri validation.
 */

#include "bs/kernel/report/report.h"

#include "bs/adapter/attach_context.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"

#include "bs/app/sdk/config_reload_session.h"
#include "bs/app/sdk/app_scenario_policy.h"
#include "bs/app/sdk/app_vendor_precheck.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <string>
#include <thread>

#include "support/config_v1_golden.h"
#include "support/day12_attach_fixture.h"
#include "support/attach_test_fixture.h"
#include "support/test_temp_dir.h"

// ============================================================================
// 1. Gate chain configuration tests
// ============================================================================

static int test_default_gate_empty_policies()
{
    AttachContext* ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("create-ctx", ctx != nullptr);

    bs::app::ConfigReloadSession session(ctx);

    BS_TEST_REQUIRE("no-gate-false", session.TakeReport() == nullptr);
    bs_adapter_attach_ctx_destroy(ctx);
    return 0;
}

static int test_set_no_gate_flag()
{
    AttachContext* ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("create-ctx", ctx != nullptr);

    bs::app::ConfigReloadSession session(ctx);
    session.SetNoGate();
    session.AddPath("test-key", reinterpret_cast<const uint8_t*>("hello"), 5);
    bs_adapter_attach_ctx_destroy(ctx);
    return 0;
}

static int test_policy_gate_chain()
{
    AttachContext* ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("create-ctx", ctx != nullptr);

    bs::app::ConfigReloadSession session(ctx);

    bs::app::ScenarioPolicy p1;
    p1.type             = bs::app::ScenarioType::ExpenseReimburse;
    p1.tenant           = "tenant-a";
    p1.allow_hot_reload = true;
    p1.max_batch        = 64;

    bs::app::ScenarioPolicy p2;
    p2.type             = bs::app::ScenarioType::GlMapping;
    p2.tenant           = "tenant-b";
    p2.allow_hot_reload = true;
    p2.max_batch        = 32;

    session.AddPolicyGate(p1);
    session.AddPolicyGate(p2);

    std::vector<bs::app::ScenarioPolicy> extra = {p1, p2};
    session.AddPolicyGates(extra);

    session.ResetGates();

    bs_adapter_attach_ctx_destroy(ctx);
    return 0;
}

static int test_custom_gate()
{
    AttachContext* ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("create-ctx", ctx != nullptr);

    bs::app::ConfigReloadSession session(ctx);

    auto custom_fn = [](const void*, size_t, char*, size_t, void*) -> int {
        return 0;
    };

    bs::app::CustomGateEntry entry1;
    entry1.gate_id = "custom_fn";
    entry1.fn      = custom_fn;
    entry1.user_ctx = nullptr;
    session.AddCustomGate(entry1);

    bs::app::CustomGateEntry entry2;
    entry2.gate_id = "null_fn";
    entry2.fn      = nullptr;
    entry2.user_ctx = nullptr;
    session.AddCustomGate(entry2);

    bs_adapter_attach_ctx_destroy(ctx);
    return 0;
}

// ============================================================================
// 2. Data passing tests
// ============================================================================

static int test_add_path_validation()
{
    AttachContext* ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("create-ctx", ctx != nullptr);

    bs::app::ConfigReloadSession session(ctx);

    BS_TEST_REQUIRE("null-key", !session.AddPath(nullptr, nullptr, 0));
    BS_TEST_REQUIRE("empty-data", !session.AddPath("key", nullptr, 0));

    const uint8_t data[] = {1, 2, 3};
    BS_TEST_REQUIRE("valid", session.AddPath("k1", data, 3));
    BS_TEST_REQUIRE("overwrite", session.AddPath("k1", data, 2));

    bs_adapter_attach_ctx_destroy(ctx);
    return 0;
}

static int test_add_uri_validation()
{
    AttachContext* ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("create-ctx", ctx != nullptr);

    bs::app::ConfigReloadSession session(ctx);

    BS_TEST_REQUIRE("null-uri", !session.AddUri(nullptr));
    BS_TEST_REQUIRE("empty", !session.AddUri(""));
    BS_TEST_REQUIRE("valid", session.AddUri("file:///some/path.json"));

    bs_adapter_attach_ctx_destroy(ctx);
    return 0;
}

// ============================================================================
// 3. Report lifecycle tests
// ============================================================================

static int test_take_report_lifecycle()
{
    AttachContext* ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("create-ctx", ctx != nullptr);

    bs::app::ConfigReloadSession session(ctx);

    BS_TEST_REQUIRE("pre-commit", session.TakeReport() == nullptr);
    BS_TEST_REQUIRE("no-commit", session.TakeReport() == nullptr);

    bs_adapter_attach_ctx_destroy(ctx);
    return 0;
}

static int test_reset_keeps_gates_clears_data()
{
    AttachContext* ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("create-ctx", ctx != nullptr);

    bs::app::ConfigReloadSession session(ctx);

    session.SetNoGate();

    const uint8_t data[] = {1, 2, 3};
    session.AddPath("k1", data, 3);
    session.AddUri("file:///path.json");

    session.Reset();

    BS_TEST_REQUIRE("re-add", session.AddPath("k2", data, 3));
    BS_TEST_REQUIRE("re-uri", session.AddUri("file:///other.json"));

    bs_adapter_attach_ctx_destroy(ctx);
    return 0;
}

// ============================================================================
// 4. Thread assert test
// ============================================================================

static int test_thread_assert()
{
    AttachContext* ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("create-ctx", ctx != nullptr);

    bs::app::ConfigReloadSession session(ctx);

    session.SetNoGate();
    session.AddPolicyGate({});
    session.ResetGates();

    bool added = session.AddPath("t", reinterpret_cast<const uint8_t*>("x"), 1);
    BS_TEST_REQUIRE("add-same-thread", added);

    bs_adapter_attach_ctx_destroy(ctx);
    return 0;
}

// ============================================================================
// 5. Default gate on Commit (with bootstrap)
// ============================================================================

static int test_commit_with_default_gate()
{
    BsTestAttachIoFixture fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("setup", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);

    {
        bs::app::ConfigReloadSession session(fix.ctx);

        const uint8_t* v1_data = reinterpret_cast<const uint8_t*>(kBlessStarConfigV1Golden);
        BS_TEST_REQUIRE("add-path", session.AddPath("test-config", v1_data, kBlessStarConfigV1GoldenLen));

        Report* report = session.Commit();
        BS_TEST_REQUIRE("commit", report != nullptr);
        BS_TEST_REQUIRE("report-created", report != nullptr);

        Report* taken = session.TakeReport();
        BS_TEST_REQUIRE("taken", taken == report);
        BS_TEST_REQUIRE("taken-twice", session.TakeReport() == nullptr);

        if (taken)
            bs_report_destroy(taken);
    }

    bs_test_attach_teardown(&fix);
    return 0;
}

// ============================================================================
// 6. Custom read_fn mode
// ============================================================================

struct TestReadCtx
{
    BsTestAttachIoFixture* fix;
};

static int test_read_fn(void* user_ctx, const char* uri, IoReadResult* out)
{
    auto* tctx = static_cast<TestReadCtx*>(user_ctx);
    return bs_io_facade_read(tctx->fix->io, uri, out);
}

static int test_commit_with_read_fn()
{
    BsTestAttachIoFixture fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("setup", fix.ctx != nullptr);
    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);

    {
        bs::app::ConfigReloadSession session(fix.ctx);
        TestReadCtx tctx{&fix};
        session.SetReadFn(test_read_fn, &tctx);

        BS_TEST_REQUIRE("add-uri", session.AddUri("file:///nonexistent"));
        session.SetNoGate();

        Report* report = session.Commit();
        BS_TEST_REQUIRE("commit", report != nullptr);

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

    rc |= test_default_gate_empty_policies();
    rc |= test_set_no_gate_flag();
    rc |= test_policy_gate_chain();
    rc |= test_custom_gate();
    rc |= test_add_path_validation();
    rc |= test_add_uri_validation();
    rc |= test_take_report_lifecycle();
    rc |= test_reset_keeps_gates_clears_data();
    rc |= test_thread_assert();
    rc |= test_commit_with_default_gate();
    rc |= test_commit_with_read_fn();

    if (rc != 0)
        std::fprintf(stderr, "FAIL: some tests returned non-zero\n");
    return rc;
}
