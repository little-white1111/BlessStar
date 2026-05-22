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
 * | D    | EventBus Publish queues; Drain delivers  | IMPL-06-04, XV-ST-01      |
 * +------+------------------------------------------+---------------------------+
 * | E    | IoFacade.read inside state callback fails  | IMPL-06-13, LOG-VII-9   |
 * +------+------------------------------------------+---------------------------+
 *
 * Labels: unit;integration;day7;io;registry;attach
 */

#include "support/config_v1_golden.h"
#include "support/day12_attach_fixture.h"

#include "bs/adapter/attach_runtime.h"
#include "bs/adapter/orchestration/reload_with_report.h"
#include "bs/adapter/registry_bootstrap.h"
#include "bs/kernel/io/io.h"
#include "bs/kernel/registry/registry_facade.h"
#include "bs/kernel/report/report.h"
#include "bs/kernel/state/ConfigEvent.h"
#include "bs/kernel/state/EventBus.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

#define REQUIRE_PHASE(phase, cond)                                                             \
    do                                                                                         \
    {                                                                                          \
        if (!(cond))                                                                           \
        {                                                                                      \
            std::fprintf(stderr, "FAIL [%s] %s:%d: (%s)\n", phase, __FILE__, __LINE__, #cond);   \
            return 1;                                                                          \
        }                                                                                      \
    } while (0)

struct AttachContext
{
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

/** Minimal bootstrap + freeze so LOG-VII-10 allows reload (see IoAttachPipelineTest for full IO read). */
static int minimal_attach_setup(AttachContext* ctx)
{
    ctx->facade = bs_registry_facade_create();
    REQUIRE_PHASE("setup", ctx->facade != nullptr);
    REQUIRE_PHASE("setup", bs_adapter_registry_bootstrap_begin(ctx->facade) == 0);
    REQUIRE_PHASE("setup", bs_adapter_attach_is_log_ready());
    REQUIRE_PHASE("setup",
                  bs_registry_facade_advance_phase(ctx->facade, BS_REGISTRY_PHASE_P2) ==
                      BS_REGISTRY_OK);
    REQUIRE_PHASE("setup", bs_adapter_registry_bootstrap_freeze(ctx->facade) == 0);
    ctx->io = bs_io_facade_create(ctx->facade);
    REQUIRE_PHASE("setup", ctx->io != nullptr);
    return 0;
}

static int prepare_reload_fixture(ReloadFixture* fix)
{
    fix->cfg_file = fs::absolute("bs_reload_gate_report_integration_cfg.txt");
    {
        std::ofstream out(fix->cfg_file, std::ios::binary);
        out.write(kBlessStarConfigV1Golden, static_cast<std::streamsize>(kBlessStarConfigV1GoldenLen));
    }
    std::string uri_path = fix->cfg_file.string();
    for (char& c : uri_path)
    {
        if (c == '\\')
            c = '/';
    }
    fix->uri = "file:///" + uri_path;
    return 0;
}

/** C: reload + default gate (no gate_fn) + Report; read path uses IoFacade. */
static int phase_c_reload_default_gate_and_report(const AttachContext* ctx, const ReloadFixture* fix)
{
    ReloadBatchController* ctrl = bs_reload_batch_controller_create(8);
    REQUIRE_PHASE("C-reload", ctrl != nullptr);

    FacadeReadCtx read_ctx{ctx->io};
    bs_reload_batch_controller_set_read_fn(ctrl, facade_read_fn, &read_ctx);
    day12_wire_reload_defaults(ctrl);
    /* gate_fn intentionally unset -> default ir_gate (IMPL-06-02) */

    Report* report = report_create("reload_default_gate_report");
    REQUIRE_PHASE("C-reload", report != nullptr);
    REQUIRE_PHASE("C-reload", bs_reload_batch_add_path(ctrl, fix->uri.c_str()) == 0);
    REQUIRE_PHASE("C-reload", bs_reload_batch_run_with_report(ctrl, report) == 0);
    REQUIRE_PHASE("C-reload", bs_reload_batch_outcome(ctrl) == BATCH_ALL_OK);

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

/** D: Publish does not run listeners; Drain does (IMPL-06-04). */
static int phase_d_eventbus_enqueue_then_drain()
{
    EventBus* bus = EventBus_Create();
    REQUIRE_PHASE("D-eventbus", bus != nullptr);

    int notified = 0;
    REQUIRE_PHASE("D-eventbus",
                  EventBus_Subscribe(bus, "/config/reload_notify", notify_listener, &notified) == 0);

    ConfigEvent* ev =
        ConfigEvent_Create("/config/reload_notify", CONFIG_EVENT_ENTER_ACTIVE,
                          CONFIG_STATE_LOADING, CONFIG_STATE_ACTIVE, 1);
    REQUIRE_PHASE("D-eventbus", ev != nullptr);
    REQUIRE_PHASE("D-eventbus", EventBus_Publish(bus, ev) == 0);
    ConfigEvent_Destroy(ev);

    REQUIRE_PHASE("D-eventbus", notified == 0);
    REQUIRE_PHASE("D-eventbus", EventBus_Drain(bus) == 0);
    REQUIRE_PHASE("D-eventbus", notified == 1);

    EventBus_Destroy(bus);
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
    auto* p = static_cast<ReentryProbe*>(user_data);
    IoReadResult tmp{};
    p->read_rc = bs_io_facade_read(p->io, "file:///reentry-blocked", &tmp);
    bs_io_read_result_free(&tmp);
}

/** E: read/reload forbidden while state callback runs (IMPL-06-13). */
static int phase_e_reentrant_read_blocked_in_callback(const AttachContext* ctx)
{
    EventBus* bus = EventBus_Create();
    REQUIRE_PHASE("E-reentry", bus != nullptr);

    ReentryProbe probe{ctx->io, BS_IO_OK};
    REQUIRE_PHASE("E-reentry",
                  EventBus_Subscribe(bus, "/config/reentry", reentry_listener, &probe) == 0);

    ConfigEvent* ev =
        ConfigEvent_Create("/config/reentry", CONFIG_EVENT_ENTER_UPDATING, CONFIG_STATE_ACTIVE,
                           CONFIG_STATE_UPDATING, 2);
    REQUIRE_PHASE("E-reentry", ev != nullptr);
    REQUIRE_PHASE("E-reentry", EventBus_Publish(bus, ev) == 0);
    ConfigEvent_Destroy(ev);
    REQUIRE_PHASE("E-reentry", EventBus_Drain(bus) == 0);
    REQUIRE_PHASE("E-reentry", probe.read_rc == BS_IO_ERR_INVALID_ARG);

    EventBus_Destroy(bus);
    return 0;
}

static void teardown(AttachContext* ctx, ReloadFixture* fix)
{
    if (ctx->io)
        bs_io_facade_destroy(ctx->io);
    if (ctx->facade)
        bs_registry_facade_destroy(ctx->facade);
    if (!fix->cfg_file.empty() && fs::exists(fix->cfg_file))
        fs::remove(fix->cfg_file);
}

int main()
{
    AttachContext ctx{};
    ReloadFixture fix{};

    if (minimal_attach_setup(&ctx) != 0)
        return 1;
    if (prepare_reload_fixture(&fix) != 0)
    {
        teardown(&ctx, &fix);
        return 1;
    }
    if (phase_c_reload_default_gate_and_report(&ctx, &fix) != 0)
    {
        teardown(&ctx, &fix);
        return 1;
    }
    if (phase_d_eventbus_enqueue_then_drain() != 0)
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
