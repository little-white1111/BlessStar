#include "bs/kernel/common/bs_reentrancy.h"
#include "bs/kernel/io/io.h"
#include "bs/kernel/registry/registry_facade.h"
#include "bs/kernel/state/ConfigEvent.h"
#include "bs/kernel/state/EventBus.h"
#include "bs/kernel/test_support/bs_test_log_bus.h"

#include "bs/adapter/attach_config.h"
#include "bs/adapter/attach_context.h"
#include "bs/adapter/registry_bootstrap.h"

#include <cassert>
#include <cstring>

static void noop_log(uint16_t, BsLogLevel, const char*, void*)
{
}

struct IoReadProbe
{
    IoFacade* facade;
    int       read_rc;
};

static void state_listener(const ConfigEvent* event, void* user_data)
{
    (void)event;
    auto*        probe = static_cast<IoReadProbe*>(user_data);
    IoReadResult tmp{};
    probe->read_rc = bs_io_facade_read(probe->facade, "file:///blocked", &tmp);
    bs_io_read_result_free(&tmp);
}

int main()
{
    assert(bs_test_log_bind_memory_bus(noop_log, nullptr) == 0);

    AttachContext* ctx = bs_adapter_attach_ctx_create();
    assert(ctx != nullptr);
    assert(bs_adapter_registry_bootstrap_begin_ctx(ctx) == 0);
    assert(bs_registry_facade_advance_phase(bs_adapter_attach_ctx_registry(ctx),
                                            BS_REGISTRY_PHASE_P2) == BS_REGISTRY_OK);
    assert(bs_adapter_registry_bootstrap_freeze_ctx(ctx) == 0);

    RegistryFacade* facade = bs_adapter_attach_ctx_registry(ctx);
    IoFacade*       io     = bs_io_facade_create(facade);
    assert(io != nullptr);

    EventBus* bus = bs_adapter_attach_config_event_bus(ctx);
    IoReadProbe    probe{io, BS_IO_OK};
    bs_event_bus_subscribe(bus, "/config/reload", state_listener, &probe);

    static const char k_payload[] = "reload-payload";
    assert(bs_adapter_attach_config_sync_path(ctx, "/config/reload", k_payload,
                                              sizeof(k_payload) - 1) == 0);

    assert(probe.read_rc == BS_IO_ERR_INVALID_ARG);

    bs_io_facade_destroy(io);
    bs_adapter_registry_shutdown_log();
    bs_adapter_attach_ctx_destroy(ctx);
    return 0;
}
