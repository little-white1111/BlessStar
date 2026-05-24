#include "bs/kernel/common/bs_reentrancy.h"
#include "bs/kernel/io/io.h"
#include "bs/kernel/registry/registry_facade.h"
#include "bs/kernel/state/ConfigEvent.h"
#include "bs/kernel/state/EventBus.h"
#include "bs/kernel/test_support/bs_test_log_bus.h"

#include "bs/adapter/attach_context.h"
#include "bs/adapter/registry_bootstrap.h"

#include <cassert>

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

    AttachContext* ctx = bs_attach_context_create();
    assert(ctx != nullptr);
    assert(bs_adapter_registry_bootstrap_begin_ctx(ctx) == 0);
    assert(bs_registry_facade_advance_phase(bs_attach_context_registry(ctx),
                                            BS_REGISTRY_PHASE_P2) == BS_REGISTRY_OK);
    assert(bs_adapter_registry_bootstrap_freeze_ctx(ctx) == 0);

    RegistryFacade* facade = bs_attach_context_registry(ctx);
    IoFacade*       io     = bs_io_facade_create(facade);
    assert(io != nullptr);

    EventBus*   bus = EventBus_Create();
    IoReadProbe probe{io, BS_IO_OK};
    EventBus_Subscribe(bus, "/config/reload", state_listener, &probe);

    ConfigEvent* ev = ConfigEvent_Create("/config/reload", CONFIG_EVENT_ENTER_LOADING,
                                         CONFIG_STATE_INITIAL, CONFIG_STATE_LOADING, 1);
    EventBus_Publish(bus, ev);
    ConfigEvent_Destroy(ev);
    EventBus_Drain(bus);

    assert(probe.read_rc == BS_IO_ERR_INVALID_ARG);

    EventBus_Destroy(bus);
    bs_io_facade_destroy(io);
    bs_adapter_registry_shutdown_log();
    bs_attach_context_destroy(ctx);
    return 0;
}
