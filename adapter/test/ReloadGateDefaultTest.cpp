/** IMPL-06-02 / M3: default gate parses v1 JSON bytes then ir_gate. */

#include "bs/kernel/report/report.h"

#include "bs/adapter/attach_runtime.h"
#include "bs/adapter/log/log_bus.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"
#include "bs/adapter/orchestration/reload_gate_default.h"
#include "bs/adapter/orchestration/reload_with_report.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <string>

#include "support/config_v1_golden.h"
#include "support/day12_attach_fixture.h"

static void noop_log(uint16_t, BsLogLevel, const char*, void*)
{
}

static int ok_read(void*, const char*, IoReadResult* out)
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

static int bad_parse_read(void*, const char*, IoReadResult* out)
{
    static const char kBad[] =
        R"({"kernel_version":"0.4.0","adapter_version":"0.4.0","instructions":[{"type":"t","name":"n","metadata":{"amount":"1","amount":"2"}}]})";
    bs_io_read_result_init(out);
    out->status = BS_IO_OK;
    out->length = sizeof(kBad) - 1;
    out->data   = static_cast<uint8_t*>(std::malloc(out->length));
    if (!out->data)
        return BS_IO_ERR_PROVIDER;
    std::memcpy(out->data, kBad, out->length);
    return BS_IO_OK;
}

int main()
{
    assert(bs_adapter_log_bind_memory_bus(noop_log, nullptr) == 0);

    ReloadBatchController* ctrl = bs_reload_batch_controller_create(4);
    assert(ctrl != nullptr);
    bs_reload_batch_controller_set_read_fn(ctrl, ok_read, nullptr);
    day12_wire_reload_defaults(ctrl);
    /* gate_fn intentionally unset -> default parse + ir_gate */

    assert(bs_reload_batch_add_path(ctrl, "file:///ok") == 0);
    assert(bs_reload_batch_run(ctrl) == 0);
    assert(bs_reload_batch_outcome(ctrl) == BATCH_ALL_OK);
    assert(bs_reload_batch_path_state(ctrl, "file:///ok") == BS_ORCH_PENDING);
    bs_reload_batch_controller_destroy(ctrl);

    ctrl = bs_reload_batch_controller_create(4);
    bs_reload_batch_controller_set_read_fn(ctrl, ok_read, nullptr);
    bs_reload_batch_controller_use_default_gate(ctrl);
    day12_wire_reload_defaults(ctrl);
    assert(bs_reload_batch_add_path(ctrl, "file:///ok2") == 0);
    assert(bs_reload_batch_run(ctrl) == 0);
    assert(bs_reload_batch_outcome(ctrl) == BATCH_ALL_OK);

    bs_reload_batch_controller_destroy(ctrl);

    ctrl = bs_reload_batch_controller_create(4);
    bs_reload_batch_controller_set_read_fn(ctrl, bad_parse_read, nullptr);
    bs_reload_batch_controller_use_default_gate(ctrl);
    day12_wire_reload_defaults(ctrl);
    Report* report = report_create("reload_gate_parse");
    assert(report != nullptr);
    bs_reload_batch_controller_set_report(ctrl, report);
    assert(bs_reload_batch_add_path(ctrl, "file:///bad-parse") == 0);
    assert(bs_reload_batch_run(ctrl) == 0);
    assert(bs_reload_batch_outcome(ctrl) == BATCH_COMPLETED_WITH_FAILURES);
    char* report_json = report_to_json(report);
    assert(report_json != nullptr);
    assert(std::strstr(report_json, "parse error at line") != nullptr);
    assert(std::strstr(report_json, "column") != nullptr);
    std::free(report_json);
    report_destroy(report);
    bs_reload_batch_controller_destroy(ctrl);

    return 0;
}
