#include "support/day12_attach_fixture.h"

#include "bs/adapter/log/log_bus.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/orchestration/reload_with_report.h"
#include "bs/kernel/report/report.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

struct MockReadCtx
{
    std::unordered_map<std::string, int> fail_uris;
};

static int mock_read(void* user_ctx, const char* uri, IoReadResult* out)
{
    auto* ctx = static_cast<MockReadCtx*>(user_ctx);
    bs_io_read_result_init(out);
    if (ctx->fail_uris.count(uri) != 0)
    {
        out->status        = BS_IO_ERR_NOT_FOUND;
        out->error_message = static_cast<char*>(std::malloc(8));
        std::strcpy(out->error_message, "fail");
        return BS_IO_ERR_NOT_FOUND;
    }
    out->status  = BS_IO_OK;
    out->length  = 1;
    out->data    = static_cast<uint8_t*>(std::malloc(1));
    out->data[0] = 'x';
    return BS_IO_OK;
}

static int mock_gate_pass(void*, const char*, const IoReadResult* read_result,
                          BsReloadGateDetail* detail_out)
{
    (void)detail_out;
    return (read_result && read_result->status == BS_IO_OK) ? 0 : -1;
}

static void noop_log(uint16_t, BsLogLevel, const char*, void*) {}

int main()
{
    assert(bs_adapter_log_bind_memory_bus(noop_log, nullptr) == 0);

    ReloadBatchController* ctrl = bs_reload_batch_controller_create(8);
    MockReadCtx read_ctx{};
    read_ctx.fail_uris["file:///bad"] = 1;
    bs_reload_batch_controller_set_read_fn(ctrl, mock_read, &read_ctx);
    bs_reload_batch_controller_set_gate_fn(ctrl, mock_gate_pass, nullptr);
    day12_wire_reload_defaults(ctrl);

    Report* report = report_create("reload_batch");
    assert(report != nullptr);

    assert(bs_reload_batch_add_path(ctrl, "file:///bad") == 0);
    assert(bs_reload_batch_run_with_report(ctrl, report) == 0);

    char* json = report_to_json(report);
    assert(json != nullptr);
    assert(std::strstr(json, "cache_attach") != nullptr);
    assert(std::strstr(json, "scheme=") != nullptr);
    assert(std::strstr(json, "abort_code=") != nullptr);
    std::free(json);

    report_destroy(report);
    bs_reload_batch_controller_destroy(ctrl);

    ctrl = bs_reload_batch_controller_create(8);
    read_ctx.fail_uris.clear();
    bs_reload_batch_controller_set_read_fn(ctrl, mock_read, &read_ctx);
    bs_reload_batch_controller_set_gate_fn(ctrl, mock_gate_pass, nullptr);
    day12_wire_reload_defaults(ctrl);

    report = report_create("reload_batch_ok");
    assert(report != nullptr);
    bs_reload_batch_controller_set_report(ctrl, report);

    assert(bs_reload_batch_add_path(ctrl, "file:///good") == 0);
    assert(bs_reload_batch_run_with_report(ctrl, report) == 0);
    assert(bs_reload_batch_outcome(ctrl) == BATCH_ALL_OK);

    json = report_to_json(report);
    assert(json != nullptr);
    assert(std::strstr(json, "persistent_commit") != nullptr);
    assert(std::strstr(json, "revision=") != nullptr);
    assert(std::strstr(json, "detail=commit_ok") != nullptr);
    std::free(json);

    report_destroy(report);
    bs_reload_batch_controller_destroy(ctrl);
    return 0;
}
