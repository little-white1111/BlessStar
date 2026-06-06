/**
 * Shared attach/bootstrap helpers for adapter integration tests.
 * Only include from adapter test .cpp files (not kernel tests).
 */

#pragma once

#include "bs/kernel/io/io.h"
#include "bs/kernel/registry/registry_facade.h"

#include "bs/adapter/attach_context.h"
#include "bs/adapter/attach_runtime.h"
#include "bs/adapter/log/log_bus.h"
#include "bs/adapter/registry_bootstrap.h"

#include <cstdio>

#include "day12_attach_fixture.h"

#define BS_TEST_REQUIRE(phase, cond)                                                               \
    do                                                                                             \
    {                                                                                              \
        if (!(cond))                                                                               \
        {                                                                                          \
            std::fprintf(stderr, "FAIL [%s] %s:%d: (%s)\n", phase, __FILE__, __LINE__, #cond);     \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

struct BsTestAttachIoFixture
{
    AttachContext*  ctx    = nullptr;
    RegistryFacade* facade = nullptr;
    IoFacade*       io     = nullptr;
};

/** RAII: push ctx as active for the current thread for the fixture lifetime. */
struct BsTestAttachActiveScope
{
    explicit BsTestAttachActiveScope(AttachContext* ctx)
        : scope_(ctx)
    {
    }
    BsTestAttachActiveScope(const BsTestAttachActiveScope&)            = delete;
    BsTestAttachActiveScope& operator=(const BsTestAttachActiveScope&) = delete;

  private:
    AttachScope scope_;
};

/** Bind reload controller to attach session + day12 defaults (integration tests). */
inline void bs_test_attach_bind_reload_ctx(ReloadBatchController* ctrl, AttachContext* ctx,
                                           BsAttachScheme scheme = BS_ATTACH_SCHEME_PER_PATH)
{
    bs_adapter_attach_reload_batch_set_attach_ctx(ctrl, ctx);
    day12_wire_reload_defaults(ctrl, scheme, ctx);
}

inline int bs_test_attach_bootstrap_begin_ctx(BsTestAttachIoFixture* fix)
{
    if (!fix || !fix->ctx)
        return -1;
    fix->facade = bs_adapter_attach_ctx_registry(fix->ctx);
    if (!fix->facade)
        return -1;
    if (bs_adapter_registry_bootstrap_begin_ctx(fix->ctx) != 0)
        return -1;
    if (!bs_adapter_attach_ctx_is_log_bus_bound(fix->ctx))
        return -1;
    return 0;
}

inline int bs_test_attach_bootstrap_freeze_ctx(BsTestAttachIoFixture* fix)
{
    if (!fix || !fix->ctx)
        return -1;
    if (bs_adapter_registry_bootstrap_freeze_ctx(fix->ctx) != 0)
        return -1;
    if (bs_registry_facade_current_phase(bs_adapter_attach_ctx_registry(fix->ctx)) !=
        BS_REGISTRY_PHASE_FROZEN)
        return -1;
    return 0;
}

inline int bs_test_attach_open_io(BsTestAttachIoFixture* fix)
{
    if (!fix || !fix->facade)
        return -1;
    fix->io = bs_io_facade_create(fix->facade);
    return fix->io ? 0 : -1;
}

inline void bs_test_attach_teardown(BsTestAttachIoFixture* fix)
{
    if (!fix)
        return;
    if (fix->io)
    {
        bs_io_facade_destroy(fix->io);
        fix->io = nullptr;
    }
    if (fix->ctx)
    {
        bs_adapter_attach_ctx_destroy(fix->ctx);
        fix->ctx    = nullptr;
        fix->facade = nullptr;
    }
    bs_adapter_registry_shutdown_log();
}
