/**
 * T-POOL-17 / IMPL-21-TEST-02: PER_BATCH 1A three-path parallel exec via KernelPool.
 */

#include "bs/adapter/attach_context.h"
#include "bs/adapter/log/log_bus.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/orchestration/reload_gate_default.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

#include "support/attach_test_fixture.h"
#include "support/config_v1_golden.h"
#include "support/day12_attach_fixture.h"

static void noop_log(uint16_t, BsLogLevel, const char*, void*)
{
}

static int golden_read(void*, const char*, IoReadResult* out)
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

int main()
{
    assert(bs_adapter_log_bind_memory_bus(noop_log, nullptr) == 0);

    BsTestAttachIoFixture fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    assert(fix.ctx != nullptr);
    BS_TEST_REQUIRE("begin", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("p2", bs_registry_facade_advance_phase(fix.facade, BS_REGISTRY_PHASE_P2) ==
                              BS_REGISTRY_OK);
    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("pool_warmed", bs_adapter_attach_ctx_is_kernel_pool_warmed(fix.ctx) == 1);

    bs_adapter_attach_ctx_set_active(fix.ctx);

    ReloadBatchController* ctrl = bs_adapter_attach_reload_batch_create(8);
    assert(ctrl != nullptr);
    bs_adapter_attach_reload_batch_set_read_fn(ctrl, golden_read, nullptr);
    bs_adapter_attach_reload_batch_set_default_gate(ctrl);
    day12_wire_reload_defaults(ctrl, BS_ATTACH_SCHEME_PER_BATCH);

    const char* uris[] = {"file:///pool/p1.json", "file:///pool/p2.json", "file:///pool/p3.json"};
    for (const char* uri : uris)
        assert(bs_adapter_attach_reload_batch_add_path(ctrl, uri) == 0);

    assert(bs_adapter_attach_reload_batch_run(ctrl) == 0);
    assert(bs_adapter_attach_reload_batch_outcome(ctrl) == BATCH_ALL_OK);

    for (const char* uri : uris)
        assert(bs_adapter_attach_reload_batch_path_state(ctrl, uri) == BS_ORCH_COMMITTED);

    bs_adapter_attach_reload_batch_destroy(ctrl);
    bs_test_attach_teardown(&fix);
    return 0;
}
