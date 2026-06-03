/**
 * IMPL-08-16 / day8: end-to-end attach chain (implemented paths only).
 *
 * AttachContext -> bootstrap_begin_ctx -> freeze_ctx (plugin loader)
 * -> resolve /adapter/io/... -> IoFacade.read -> bs_status_format
 * -> reload (default ir_gate) + Report
 */

#include "bs/kernel/common/bs_status.h"
#include "bs/kernel/io/io_status_table.h"
#include "bs/kernel/report/report.h"

#include "bs/adapter/orchestration/reload_with_report.h"

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
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_day8_attach_full"));
    const fs::path&          work = tmp_guard.path;

    BsTestAttachIoFixture fix{};
    fix.ctx = bs_adapter_attach_ctx_create();
    BS_TEST_REQUIRE("setup", fix.ctx != nullptr);

    BS_TEST_REQUIRE("bootstrap", bs_test_attach_bootstrap_begin_ctx(&fix) == 0);
    BS_TEST_REQUIRE("phase", bs_registry_facade_current_phase(fix.facade) == BS_REGISTRY_PHASE_P1);

    BS_TEST_REQUIRE("freeze", bs_test_attach_bootstrap_freeze_ctx(&fix) == 0);
    BS_TEST_REQUIRE("open-io", bs_test_attach_open_io(&fix) == 0);

    Binding local{};
    BS_TEST_REQUIRE("resolve", bs_registry_facade_resolve(fix.facade, "/adapter/io/local",
                                                          &local) == BS_REGISTRY_OK);
    Binding db{};
    BS_TEST_REQUIRE("resolve",
                    bs_registry_facade_resolve(fix.facade, "adapter.io.db", &db) == BS_REGISTRY_OK);
    Binding remote{};
    BS_TEST_REQUIRE("resolve", bs_registry_facade_resolve(fix.facade, "/adapter/io/remote",
                                                          &remote) == BS_REGISTRY_OK);

    const fs::path cfg_file = work / "cfg.json";
    BS_TEST_REQUIRE("write-cfg", bs_test_write_binary_file(cfg_file, kBlessStarConfigV1Golden,
                                                           kBlessStarConfigV1GoldenLen));
    const std::string uri = bs_test_path_to_file_uri(cfg_file);

    IoReadResult read_result{};
    BS_TEST_REQUIRE("io-read", bs_io_facade_read(fix.io, uri.c_str(), &read_result) == BS_IO_OK);
    BS_TEST_REQUIRE("io-read", read_result.length == kBlessStarConfigV1GoldenLen);
    BS_TEST_REQUIRE("io-read", read_result.data != nullptr);
    BS_TEST_REQUIRE("io-read", std::memcmp(read_result.data, kBlessStarConfigV1Golden,
                                           kBlessStarConfigV1GoldenLen) == 0);
    bs_io_read_result_free(&read_result);

    uint16_t                   io_domain = 0;
    BsStatusDomainRegistration reg{};
    reg.domain_qname  = "io";
    reg.table         = k_io_status_table;
    reg.table_len     = k_io_status_table_len;
    reg.out_domain_id = &io_domain;
    BS_TEST_REQUIRE("domain", bs_registry_facade_register_status_domain(fix.facade, &reg) ==
                                  BS_REGISTRY_ERR_FROZEN);

    char           fmt_buf[64];
    const BsStatus timeout_status = bs_status_make(1, 5);
    BS_TEST_REQUIRE("format",
                    bs_status_format(timeout_status, fix.facade, fmt_buf, sizeof(fmt_buf)) == 0);
    BS_TEST_REQUIRE("format", std::strcmp(fmt_buf, "io.TIMEOUT") == 0);

    ReloadBatchController* ctrl = bs_adapter_attach_reload_batch_create(8);
    BS_TEST_REQUIRE("reload", ctrl != nullptr);
    bs_adapter_attach_reload_batch_set_read_fn(ctrl, facade_read_fn, &fix);
    const fs::path manifest_path = work / "manifest.bs";
    bs_adapter_attach_reload_batch_set_attach_scheme(ctrl, BS_ATTACH_SCHEME_PER_PATH);
    bs_adapter_attach_reload_batch_set_manifest_path(ctrl, manifest_path.string().c_str());
    Report* report = bs_report_create("day8_attach_full");
    BS_TEST_REQUIRE("reload", report != nullptr);
    BS_TEST_REQUIRE("reload", bs_adapter_attach_reload_batch_add_path(ctrl, uri.c_str()) == 0);
    BS_TEST_REQUIRE("reload", bs_adapter_attach_reload_batch_run_with_report(ctrl, report) == 0);
    BS_TEST_REQUIRE("reload", bs_adapter_attach_reload_batch_outcome(ctrl) == BATCH_ALL_OK);

    char* json = bs_report_to_json(report);
    BS_TEST_REQUIRE("reload", json != nullptr);
    std::free(json);

    bs_report_destroy(report);
    bs_adapter_attach_reload_batch_destroy(ctrl);

    bs_test_attach_teardown(&fix);
    return 0;
}
