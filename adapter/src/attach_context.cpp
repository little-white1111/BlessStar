#include "bs/adapter/attach_context.h"

#include <cstdlib>
#include <cstring>

struct AttachContext
{
    RegistryFacade* registry;
    BsLogState      log_state;
    int             log_bus_bound;
    int             owns_registry;
};

static AttachContext* g_active_ctx = nullptr;
static AttachContext  g_ephemeral_log_ctx;
static AttachContext  g_legacy_bootstrap_ctx;
static int            g_ephemeral_initialized = 0;
static int            g_legacy_initialized    = 0;

static void ensure_ephemeral_initialized(void)
{
    if (!g_ephemeral_initialized)
    {
        bs_log_state_init(&g_ephemeral_log_ctx.log_state);
        g_ephemeral_log_ctx.log_bus_bound = 0;
        g_ephemeral_log_ctx.owns_registry = 0;
        g_ephemeral_log_ctx.registry      = nullptr;
        g_ephemeral_initialized           = 1;
    }
}

AttachContext* bs_attach_context_create(void)
{
    auto* ctx = static_cast<AttachContext*>(std::calloc(1, sizeof(AttachContext)));
    if (!ctx)
        return nullptr;

    ctx->registry = bs_registry_facade_create();
    if (!ctx->registry)
    {
        std::free(ctx);
        return nullptr;
    }

    ctx->owns_registry  = 1;
    ctx->log_bus_bound  = 0;
    bs_log_state_init(&ctx->log_state);
    return ctx;
}

void bs_attach_context_destroy(AttachContext* ctx)
{
    if (!ctx)
        return;

    if (g_active_ctx == ctx)
    {
        g_active_ctx = nullptr;
        bs_log_set_current_state(nullptr);
    }

    if (ctx->owns_registry && ctx->registry)
        bs_registry_facade_destroy(ctx->registry);

    std::memset(ctx, 0, sizeof(*ctx));
    std::free(ctx);
}

RegistryFacade* bs_attach_context_registry(AttachContext* ctx)
{
    return ctx ? ctx->registry : nullptr;
}

BsLogState* bs_attach_context_log_state(AttachContext* ctx)
{
    return ctx ? &ctx->log_state : nullptr;
}

int bs_attach_context_is_log_bus_bound(const AttachContext* ctx)
{
    return ctx && ctx->log_bus_bound != 0;
}

void bs_attach_context_set_log_bus_bound(AttachContext* ctx, int bound)
{
    if (!ctx)
        return;
    ctx->log_bus_bound = bound ? 1 : 0;
}

BsLogLevel bs_attach_context_get_log_level(const AttachContext* ctx)
{
    return ctx ? bs_log_get_global_level_ctx(&ctx->log_state) : BS_LOG_INFO;
}

void bs_attach_context_set_log_level(AttachContext* ctx, BsLogLevel level)
{
    if (!ctx)
        return;
    bs_log_set_global_level_ctx(&ctx->log_state, level);
}

void bs_attach_context_set_active(AttachContext* ctx)
{
    g_active_ctx = ctx;
    bs_log_set_current_state(ctx ? &ctx->log_state : nullptr);
}

AttachContext* bs_attach_context_get_active(void)
{
    return g_active_ctx;
}

void bs_adapter_attach_ensure_active_ctx(void)
{
    if (!g_active_ctx)
    {
        ensure_ephemeral_initialized();
        g_active_ctx = &g_ephemeral_log_ctx;
        bs_log_set_current_state(&g_ephemeral_log_ctx.log_state);
    }
}

void bs_attach_context_use_external_registry(AttachContext* ctx, RegistryFacade* facade)
{
    if (!ctx)
        return;
    const int registry_changed = (ctx->registry != facade);
    ctx->registry              = facade;
    ctx->owns_registry         = 0;
    if (registry_changed)
    {
        ctx->log_bus_bound = 0;
        bs_log_state_init(&ctx->log_state);
    }
}

AttachContext* bs_attach_context_legacy_bootstrap(void)
{
    if (!g_legacy_initialized)
    {
        bs_log_state_init(&g_legacy_bootstrap_ctx.log_state);
        g_legacy_bootstrap_ctx.registry       = nullptr;
        g_legacy_bootstrap_ctx.owns_registry  = 0;
        g_legacy_bootstrap_ctx.log_bus_bound  = 0;
        g_legacy_initialized                  = 1;
    }
    return &g_legacy_bootstrap_ctx;
}
