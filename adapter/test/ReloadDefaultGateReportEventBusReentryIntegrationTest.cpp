/**
 * Integration: Day 7 incremental chains on a minimal attach setup.
 *
 * Prerequisites (covered elsewhere, not re-tested here):
 *   - IoAttachPipelineTest: bootstrap + freeze + IoFacade.read
 *   - AttachPipelineRegistryTest: registry P2 plugin + IR gate + resolve
 *
 * +------+------------------------------------------+---------------------------+
 * | Step | What this file tests                     | Architecture anchor       |
 * +------+------------------------------------------+---------------------------+
 * | C    | reload batch, default ir_gate, Report JSON | IMPL-06-02, IMPL-06-05   |
 * |      | (read via IoFacade; no gate_fn injected) | LOG-VII-10 (bind required)|
 * +------+------------------------------------------+---------------------------+
 * | D    | ConfigManager EventBus: Publish queues;  | IMPL-06-04, XV-ST-01, B-05|
 * |      | Drain delivers ENTER_ACTIVE listeners    |                           |
 * +------+------------------------------------------+---------------------------+
 * | E    | IoFacade.read inside state callback fails  | IMPL-06-13, LOG-VII-9   |
 * +------+------------------------------------------+---------------------------+
 *
 * Labels: unit;integration;day7;io;registry;attach
 */

#include "bs/kernel/io/io.h"
#include "bs/kernel/registry/registry_facade.h"
#include "bs/kernel/report/report.h"
#include "bs/kernel/state/ConfigEvent.h"
#include "bs/kernel/state/ConfigState.h"
#include "bs/kernel/state/EventBus.h"

#include "bs/adapter/attach_config.h"
#include "bs/adapter/attach_context.h"
#include "bs/adapter/attach_runtime.h"
#include "bs/adapter/orchestration/reload_with_report.h"
#include "bs/adapter/registry_bootstrap.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <string>

#include "support/config_v1_golden.h"
#include "support/day12_attach_fixture.h"
#include "support/test_temp_dir.h"

namespace fs = std::filesystem;

#define REQUIRE_PHASE(phase, cond)                                                                 \
    do                                                                                             \
    {                                                                                              \
        if (!(cond))                                                                               \
        {                                                                                          \
            std::fprintf(stderr, "FAIL [%s] %s:%d: (%s)\n", phase, __FILE__, __LINE__, #cond);     \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

struct ReloadHarness
{
    AttachContext*  actx   = nullptr;
    RegistryFacade* facade = nullptr;
    IoFacade*       io     = nullptr;
};

struct ReloadFixture
{
    fs::path    cfg_file;
    std::string uri;
};

struct FacadeReadCtx
{
    IoFacade* io;
};

static int facade_read_fn(void* user_ctx, const char* uri, IoReadResult* out)
{
    auto* ctx = static_cast<FacadeReadCtx*>(user_ctx);
    return bs_io_facade_read(ctx->io, uri, out);
}

/** Minimal bootstrap + freeze so LOG-VII-10 allows reload (see IoAttachPipelineTest for full IO
 * read). */
static int minimal_attach_setup(ReloadHarness* h)
{
    h->actx = bs_attach_context_create();
    REQUIRE_PHASE("setup", h->actx != nullptr);
    bs_attach_context_set_active(h->actx);
    REQUIRE_PHASE("setup", bs_adapter_registry_bootstrap_begin_ctx(h->actx) == 0);
    REQUIRE_PHASE("setup", bs_adapter_attach_is_log_ready());
    h->facade = bs_attach_context_registry(h->actx);
    REQUIRE_PHASE("setup", bs_registry_facade_advance_phase(h->facade, BS_REGISTRY_PHASE_P2) ==
                               BS_REGISTRY_OK);
    REQUIRE_PHASE("setup", bs_adapter_registry_bootstrap_freeze_ctx(h->actx) == 0);
    h->io = bs_io_facade_create(h->facade);
    REQUIRE_PHASE("setup", h->io != nullptr);
    return 0;
}

static int prepare_reload_fixture(ReloadFixture* fix, const fs::path& work)
{
    fix->cfg_file = work / "integration_cfg.txt";
    if (!bs_test_write_binary_file(fix->cfg_file, kBlessStarConfigV1Golden,
                                   kBlessStarConfigV1GoldenLen))
        return 1;
    fix->uri = bs_test_path_to_file_uri(fix->cfg_file);
    return 0;
}

/** C: reload + default gate (no gate_fn) + Report; read path uses IoFacade. */
static int phase_c_reload_default_gate_and_report(const ReloadHarness* h, const ReloadFixture* fix)
{
    ReloadBatchController* ctrl = bs_reload_batch_controller_create(8);
    REQUIRE_PHASE("C-reload", ctrl != nullptr);

    FacadeReadCtx read_ctx{h->io};
    bs_reload_batch_controller_set_read_fn(ctrl, facade_read_fn, &read_ctx);
    day12_wire_reload_defaults(ctrl);
    /* gate_fn intentionally unset -> default ir_gate (IMPL-06-02) */

    Report* report = report_create("reload_default_gate_report");
    REQUIRE_PHASE("C-reload", report != nullptr);
    REQUIRE_PHASE("C-reload", bs_reload_batch_add_path(ctrl, fix->uri.c_str()) == 0);
    REQUIRE_PHASE("C-reload", bs_reload_batch_run_with_report(ctrl, report) == 0);
    REQUIRE_PHASE("C-reload", bs_reload_batch_outcome(ctrl) == BATCH_ALL_OK);

    ConfigState st = CONFIG_STATE_INITIAL;
    REQUIRE_PHASE("C-reload",
                  bs_adapter_attach_config_get_state(h->actx, fix->uri.c_str(), &st) == 0);
    REQUIRE_PHASE("C-reload", st == CONFIG_STATE_ACTIVE);

    char* json = report_to_json(report);
    REQUIRE_PHASE("C-reload", json != nullptr);
    std::free(json);

    report_destroy(report);
    bs_reload_batch_controller_destroy(ctrl);
    return 0;
}

static void notify_listener(const ConfigEvent* event, void* user_data)
{
    (void)event;
    ++*static_cast<int*>(user_data);
}

/** D: Publish does not run listeners; Drain does (IMPL-06-04) via ConfigManager EventBus. */
static int phase_d_eventbus_enqueue_then_drain(AttachContext* actx)
{
    EventBus* bus = bs_adapter_attach_config_event_bus(actx);
    REQUIRE_PHASE("D-eventbus", bus != nullptr);

    int notified = 0;
    REQUIRE_PHASE("D-eventbus", EventBus_Subscribe(bus, "/config/reload_notify",
                                                       notify_listener, &notified) == 0);

    ConfigEvent* ev = ConfigEvent_Create("/config/reload_notify", CONFIG_EVENT_ENTER_ACTIVE,
                                             CONFIG_STATE_LOADING, CONFIG_STATE_ACTIVE, 1);
    REQUIRE_PHASE("D-eventbus", ev != nullptr);
    REQUIRE_PHASE("D-eventbus", EventBus_Publish(bus, ev) == 0);
    ConfigEvent_Destroy(ev);

    REQUIRE_PHASE("D-eventbus", notified == 0);
    REQUIRE_PHASE("D-eventbus", EventBus_Drain(bus) == 0);
    REQUIRE_PHASE("D-eventbus", notified == 1);

    return 0;
}

struct ReentryProbe
{
    IoFacade* io;
    int       read_rc;
};

static void reentry_listener(const ConfigEvent* event, void* user_data)
{
    (void)event;
    auto*        p = static_cast<ReentryProbe*>(user_data);
    IoReadResult tmp{};
    p->read_rc = bs_io_facade_read(p->io, "file:///reentry-blocked", &tmp);
    bs_io_read_result_free(&tmp);
}

/** E: read/reload forbidden while state callback runs (IMPL-06-13). */
static int phase_e_reentrant_read_blocked_in_callback(const ReloadHarness* h)
{
    EventBus* bus = bs_adapter_attach_config_event_bus(h->actx);
    REQUIRE_PHASE("E-reentry", bus != nullptr);

    ReentryProbe probe{h->io, BS_IO_OK};
    REQUIRE_PHASE("E-reentry",
                  EventBus_Subscribe(bus, "/config/reentry", reentry_listener, &probe) == 0);

    ConfigEvent* ev = ConfigEvent_Create("/config/reentry", CONFIG_EVENT_ENTER_UPDATING,
                                             CONFIG_STATE_ACTIVE, CONFIG_STATE_UPDATING, 2);
    REQUIRE_PHASE("E-reentry", ev != nullptr);
    REQUIRE_PHASE("E-reentry", EventBus_Publish(bus, ev) == 0);
    ConfigEvent_Destroy(ev);
    REQUIRE_PHASE("E-reentry", EventBus_Drain(bus) == 0);
    REQUIRE_PHASE("E-reentry", probe.read_rc == BS_IO_ERR_INVALID_ARG);

    return 0;
}

static void teardown(ReloadHarness* h, ReloadFixture* fix)
{
    if (h->io)
        bs_io_facade_destroy(h->io);
    bs_adapter_registry_shutdown_log();
    if (h->actx)
        bs_attach_context_destroy(h->actx);
}

int main()
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_reload_gate_report"));
    ReloadHarness            ctx{};
    ReloadFixture            fix{};

    if (minimal_attach_setup(&ctx) != 0)
        return 1;
    if (prepare_reload_fixture(&fix, tmp_guard.path) != 0)
    {
        teardown(&ctx, &fix);
        return 1;
    }
    if (phase_c_reload_default_gate_and_report(&ctx, &fix) != 0)
    {
        teardown(&ctx, &fix);
        return 1;
    }
    if (phase_d_eventbus_enqueue_then_drain(ctx.actx) != 0)
    {
        teardown(&ctx, &fix);
        return 1;
    }
    if (phase_e_reentrant_read_blocked_in_callback(&ctx) != 0)
    {
        teardown(&ctx, &fix);
        return 1;
    }

    teardown(&ctx, &fix);
    return 0;
}
