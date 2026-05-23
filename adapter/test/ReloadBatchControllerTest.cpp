#include "bs/adapter/log/log_bus.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/orchestration/reload_gate_default.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <string>
#include <unordered_map>

#include "support/config_v1_golden.h"
#include "support/day12_attach_fixture.h"

struct MockReadCtx
{
    std::unordered_map<std::string, int> fail_uris;
    std::unordered_map<std::string, int> fail_through_attempt;
    std::unordered_map<std::string, int> attempts;
    int                                  calls = 0;
};

static int mock_read(void* user_ctx, const char* uri, IoReadResult* out)
{
    auto* ctx = static_cast<MockReadCtx*>(user_ctx);
    ++ctx->calls;
    bs_io_read_result_init(out);

    const int  attempt = ++ctx->attempts[uri];
    const auto until   = ctx->fail_through_attempt.find(uri);
    if (until != ctx->fail_through_attempt.end() && attempt <= until->second)
    {
        out->status        = BS_IO_ERR_NOT_FOUND;
        out->error_message = static_cast<char*>(std::malloc(8));
        std::strcpy(out->error_message, "retry");
        return BS_IO_ERR_NOT_FOUND;
    }

    if (ctx->fail_uris.count(uri) != 0)
    {
        out->status        = BS_IO_ERR_NOT_FOUND;
        out->error_message = static_cast<char*>(std::malloc(8));
        std::strcpy(out->error_message, "fail");
        return BS_IO_ERR_NOT_FOUND;
    }
    out->status = BS_IO_OK;
    out->length = kBlessStarConfigV1GoldenLen;
    out->data   = static_cast<uint8_t*>(std::malloc(out->length));
    if (!out->data)
        return BS_IO_ERR_PROVIDER;
    std::memcpy(out->data, kBlessStarConfigV1Golden, out->length);
    return BS_IO_OK;
}

static int mock_gate_pass(void* user_ctx, const char* uri, const IoReadResult* read_result,
                          BsReloadGateDetail* detail_out)
{
    (void)user_ctx;
    (void)uri;
    (void)detail_out;
    return (read_result && read_result->status == BS_IO_OK) ? 0 : -1;
}

struct MockGateCtx
{
    std::unordered_map<std::string, int> reject_uris;
};

static int mock_gate(void* user_ctx, const char* uri, const IoReadResult* read_result,
                     BsReloadGateDetail* detail_out)
{
    auto* ctx = static_cast<MockGateCtx*>(user_ctx);
    (void)read_result;
    (void)detail_out;
    if (ctx->reject_uris.count(uri) != 0)
        return BS_RELOAD_GATE_IR_REJECT;
    return BS_RELOAD_GATE_OK;
}

static void test_log_sink(uint16_t, BsLogLevel, const char*, void*)
{
}

int main()
{
    assert(bs_adapter_log_bind_memory_bus(test_log_sink, nullptr) == 0);

    ReloadBatchController* ctrl = bs_reload_batch_controller_create(2);
    assert(ctrl != nullptr);

    MockReadCtx read_ctx{};
    bs_reload_batch_controller_set_read_fn(ctrl, mock_read, &read_ctx);
    bs_reload_batch_controller_set_gate_fn(ctrl, mock_gate_pass, nullptr);
    day12_wire_reload_defaults(ctrl);

    assert(bs_reload_batch_add_path(ctrl, "file:///a") == 0);
    assert(bs_reload_batch_add_path(ctrl, "file:///b") == 0);
    assert(bs_reload_batch_add_path(ctrl, "file:///c") == -1);

    assert(bs_reload_batch_run(ctrl) == 0);
    assert(bs_reload_batch_outcome(ctrl) == BATCH_ALL_OK);
    assert(read_ctx.calls == 2);

    bs_reload_batch_controller_destroy(ctrl);

    /* XV-IO-02: one read fail does not skip the other path */
    ctrl                              = bs_reload_batch_controller_create(8);
    read_ctx                          = MockReadCtx{};
    read_ctx.fail_uris["file:///bad"] = 1;
    bs_reload_batch_controller_set_read_fn(ctrl, mock_read, &read_ctx);
    bs_reload_batch_controller_set_gate_fn(ctrl, mock_gate_pass, nullptr);
    day12_wire_reload_defaults(ctrl);

    assert(bs_reload_batch_add_path(ctrl, "file:///bad") == 0);
    assert(bs_reload_batch_add_path(ctrl, "file:///good") == 0);
    assert(bs_reload_batch_run(ctrl) == 0);
    assert(bs_reload_batch_outcome(ctrl) == BATCH_COMPLETED_WITH_FAILURES);
    assert(read_ctx.calls == 2);
    assert(bs_reload_batch_path_state(ctrl, "file:///bad") == BS_ORCH_PENDING);
    assert(bs_reload_batch_path_state(ctrl, "file:///good") == BS_ORCH_PENDING);

    bs_reload_batch_controller_destroy(ctrl);

    /* Gate reject: read ok but gate fails -> GATE_REJECTED; other path still runs */
    ctrl     = bs_reload_batch_controller_create(8);
    read_ctx = MockReadCtx{};
    MockGateCtx gate_ctx{};
    gate_ctx.reject_uris["file:///gated"] = 1;
    bs_reload_batch_controller_set_read_fn(ctrl, mock_read, &read_ctx);
    bs_reload_batch_controller_set_gate_fn(ctrl, mock_gate, &gate_ctx);
    day12_wire_reload_defaults(ctrl);

    assert(bs_reload_batch_add_path(ctrl, "file:///gated") == 0);
    assert(bs_reload_batch_add_path(ctrl, "file:///ok") == 0);
    assert(bs_reload_batch_run(ctrl) == 0);
    assert(bs_reload_batch_outcome(ctrl) == BATCH_COMPLETED_WITH_FAILURES);
    assert(read_ctx.calls == 2);

    bs_reload_batch_controller_destroy(ctrl);

    /* Retry: fail once then succeed when max_retry=1 */
    ctrl                                           = bs_reload_batch_controller_create(8);
    read_ctx                                       = MockReadCtx{};
    read_ctx.fail_through_attempt["file:///retry"] = 1;
    bs_reload_batch_controller_set_read_fn(ctrl, mock_read, &read_ctx);
    bs_reload_batch_controller_set_gate_fn(ctrl, mock_gate_pass, nullptr);
    bs_reload_batch_controller_set_max_retry(ctrl, 1);
    day12_wire_reload_defaults(ctrl);

    assert(bs_reload_batch_add_path(ctrl, "file:///retry") == 0);
    assert(bs_reload_batch_run(ctrl) == 0);
    assert(bs_reload_batch_outcome(ctrl) == BATCH_ALL_OK);
    assert(read_ctx.calls == 2);

    bs_reload_batch_controller_destroy(ctrl);
    return 0;
}
