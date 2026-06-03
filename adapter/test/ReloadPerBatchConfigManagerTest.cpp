/**
 * IMPL-17-CM-05 / T6: PER_BATCH ConfigManager tentative sync, commit authoritative, abort rollback.
 */

#include "bs/kernel/state/ConfigState.h"

#include "bs/adapter/attach_config.h"
#include "bs/adapter/attach_context.h"
#include "bs/adapter/log/log_bus.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/orchestration/reload_gate_default.h"
#include "bs/adapter/registry_bootstrap.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "support/config_v1_golden.h"
#include "support/day12_attach_fixture.h"

static void test_log_sink(uint16_t, BsLogLevel, const char*, void*)
{
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

static int reject_uri_gate(void* user_ctx, const char* uri, const IoReadResult* read_result,
                           BsReloadGateDetail* detail_out)
{
    (void)read_result;
    (void)detail_out;
    const char* bad = static_cast<const char*>(user_ctx);
    if (bad && uri && std::strcmp(uri, bad) == 0)
        return BS_RELOAD_GATE_IR_REJECT;
    return BS_RELOAD_GATE_OK;
}

static int snapshot_equals(AttachContext* ctx, const char* path, const void* expect,
                           size_t expect_len)
{
    void*  snap = nullptr;
    size_t sz   = 0;
    if (bs_adapter_attach_config_get_snapshot(ctx, path, &snap, &sz) != 0)
        return 0;
    const int ok =
        (sz == expect_len && snap && expect && std::memcmp(snap, expect, sz) == 0) ? 1 : 0;
    std::free(snap);
    return ok;
}

static int mock_gate_pass(void* /*user_ctx*/, const char* /*uri*/, const IoReadResult* read_result,
                          BsReloadGateDetail* /*detail_out*/)
{
    return (read_result && read_result->status == BS_IO_OK) ? BS_RELOAD_GATE_OK
                                                            : BS_RELOAD_GATE_IR_REJECT;
}

static int ensure_ctx_kernel_ready(AttachContext* ctx)
{
    RegistryFacade* const facade = bs_adapter_attach_ctx_registry(ctx);
    if (facade && bs_registry_facade_current_phase(facade) == BS_REGISTRY_PHASE_FROZEN)
        return 0;

    if (bs_adapter_registry_bootstrap_begin_ctx(ctx) != 0)
        return -1;
    if (bs_registry_facade_advance_phase(facade, BS_REGISTRY_PHASE_P2) != BS_REGISTRY_OK)
        return -1;
    return bs_adapter_registry_bootstrap_freeze_ctx(ctx);
}

static int run_per_batch_with_ctx(AttachContext* ctx, ReloadBatchController* ctrl,
                                  const char* reject_uri)
{
    if (ensure_ctx_kernel_ready(ctx) != 0)
        return -1;

    bs_adapter_attach_ctx_set_active(ctx);
    bs_adapter_attach_ctx_set_log_bus_bound(ctx, 1);
    bs_adapter_attach_reload_batch_set_read_fn(ctrl, golden_read, nullptr);
    if (reject_uri)
        bs_adapter_attach_reload_batch_set_gate_fn(ctrl, reject_uri_gate,
                                                   const_cast<char*>(reject_uri));
    else
        bs_adapter_attach_reload_batch_set_gate_fn(ctrl, mock_gate_pass, nullptr);
    day12_wire_reload_defaults(ctrl, BS_ATTACH_SCHEME_PER_BATCH);
    return bs_adapter_attach_reload_batch_run(ctrl);
}

static int test_per_batch_commit_syncs_cm(void)
{
    AttachContext* ctx = bs_adapter_attach_ctx_create();
    if (!ctx)
        return 1;
    bs_adapter_attach_ctx_set_active(ctx);

    ReloadBatchController* ctrl = bs_adapter_attach_reload_batch_create(8);
    const char*            uri  = "file:///cfg/per-batch-ok.json";
    bs_adapter_attach_reload_batch_add_path(ctrl, uri);

    if (run_per_batch_with_ctx(ctx, ctrl, nullptr) != 0)
        return 1;
    if (bs_adapter_attach_reload_batch_outcome(ctrl) != BATCH_ALL_OK)
        return 1;

    ConfigState st = CONFIG_STATE_INITIAL;
    if (bs_adapter_attach_config_get_state(ctx, uri, &st) != 0 || st != CONFIG_STATE_ACTIVE)
        return 1;
    if (!snapshot_equals(ctx, uri, kBlessStarConfigV1Golden, kBlessStarConfigV1GoldenLen))
        return 1;

    bs_adapter_attach_reload_batch_destroy(ctrl);
    bs_adapter_attach_ctx_destroy(ctx);
    return 0;
}

static int test_per_batch_abort_rollback(void)
{
    static const char k_prior[] = "prior-config-v1";
    const char*       uri_ok    = "file:///cfg/prior.json";
    const char*       uri_bad   = "file:///cfg/bad-gate.json";

    AttachContext* ctx = bs_adapter_attach_ctx_create();
    if (!ctx)
        return 1;
    if (bs_adapter_attach_config_sync_path(ctx, uri_ok, k_prior, sizeof(k_prior) - 1) != 0)
        return 1;

    ReloadBatchController* ctrl = bs_adapter_attach_reload_batch_create(8);
    bs_adapter_attach_reload_batch_add_path(ctrl, uri_ok);
    bs_adapter_attach_reload_batch_add_path(ctrl, uri_bad);

    if (run_per_batch_with_ctx(ctx, ctrl, uri_bad) != 0)
        return 1;
    if (bs_adapter_attach_reload_batch_outcome(ctrl) != BATCH_COMPLETED_WITH_FAILURES)
        return 1;
    if (!snapshot_equals(ctx, uri_ok, k_prior, sizeof(k_prior) - 1))
        return 1;

    bs_adapter_attach_reload_batch_destroy(ctrl);
    bs_adapter_attach_ctx_destroy(ctx);
    return 0;
}

#if defined(BS_TESTING)
static int test_sync_fail_maps_persist_rejected(void)
{
    const char* uri = "file:///cfg/sync-fail.json";

    AttachContext* ctx = bs_adapter_attach_ctx_create();
    if (!ctx)
        return 1;

    ReloadBatchController* ctrl = bs_adapter_attach_reload_batch_create(4);
    bs_adapter_attach_reload_batch_add_path(ctrl, uri);

    bs_adapter_attach_config_testing_set_sync_fail_path(uri);
    bs_adapter_attach_ctx_set_active(ctx);
    bs_adapter_attach_ctx_set_log_bus_bound(ctx, 1);
    bs_adapter_attach_reload_batch_set_read_fn(ctrl, golden_read, nullptr);
    bs_adapter_attach_reload_batch_set_gate_fn(ctrl, mock_gate_pass, nullptr);
    day12_wire_reload_defaults(ctrl, BS_ATTACH_SCHEME_PER_PATH);

    if (bs_adapter_attach_reload_batch_run(ctrl) != 0)
        return 1;
    if (bs_adapter_attach_reload_batch_outcome(ctrl) != BATCH_COMPLETED_WITH_FAILURES)
        return 1;

    ConfigState st = CONFIG_STATE_INITIAL;
    if (bs_adapter_attach_config_get_state(ctx, uri, &st) != -2)
        return 1;

    bs_adapter_attach_config_testing_clear_sync_fail_path();
    bs_adapter_attach_reload_batch_destroy(ctrl);
    bs_adapter_attach_ctx_destroy(ctx);
    return 0;
}
#endif

int main()
{
    if (bs_adapter_log_bind_memory_bus(test_log_sink, nullptr) != 0)
        return 1;

    if (test_per_batch_commit_syncs_cm() != 0)
        return 1;
    if (test_per_batch_abort_rollback() != 0)
        return 1;
#if defined(BS_TESTING)
    if (test_sync_fail_maps_persist_rejected() != 0)
        return 1;
#endif
    bs_adapter_registry_shutdown_log();
    return 0;
}
