#include "bs/kernel/ir/ir.h"
#include "bs/kernel/pipeline/Stage.h"
#include "bs/kernel/pipeline/pipeline.h"
#include "bs/kernel/registry/types.h"
#include "bs/kernel/runtime/Kernel.h"

#include "bs/adapter/attach_config.h"
#include "bs/adapter/attach_context.h"
#include "bs/adapter/attach_ir_snapshot.h"
#include "bs/adapter/attach_runtime.h"
#include "bs/adapter/attach_session.h"
#include "bs/adapter/log/log_bus.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <mutex>
#include <vector>

#include "attach_context_internal.h"

static int attach_audit_stage_execute(Stage* /*stage*/, const IRInstruction* input,
                                      IRInstruction** output)
{
    if (!input || !input->type)
        return -1;
    if (output)
        *output = nullptr;
    return 0;
}

static thread_local std::vector<AttachContext*> g_active_ctx_stack;
static thread_local int                         g_active_ctx_access_depth = 0;
static thread_local AttachContext               g_ephemeral_log_ctx;
static thread_local int                         g_ephemeral_initialized = 0;
static AttachContext                            g_legacy_bootstrap_ctx;
static int                                      g_legacy_initialized = 0;

static AttachContext* active_ctx_top(void)
{
    return g_active_ctx_stack.empty() ? nullptr : g_active_ctx_stack.back();
}

static void sync_log_state_from_active(void)
{
    AttachContext* top = active_ctx_top();
    bs_log_set_current_state(top ? &top->log_state : nullptr);
}

static void pop_active_ctx_if_present(AttachContext* ctx)
{
    if (g_active_ctx_stack.empty())
        return;
    if (ctx == nullptr)
    {
        g_active_ctx_stack.pop_back();
        sync_log_state_from_active();
        return;
    }
    if (g_active_ctx_stack.back() == ctx)
    {
        g_active_ctx_stack.pop_back();
        sync_log_state_from_active();
        return;
    }
    for (auto it = g_active_ctx_stack.begin(); it != g_active_ctx_stack.end(); ++it)
    {
        if (*it == ctx)
        {
            g_active_ctx_stack.erase(it);
            sync_log_state_from_active();
            break;
        }
    }
}

static void ensure_ephemeral_initialized(void)
{
    if (!g_ephemeral_initialized)
    {
        bs_log_state_init(&g_ephemeral_log_ctx.log_state);
        g_ephemeral_log_ctx.log_bus_bound  = 0;
        g_ephemeral_log_ctx.owns_registry  = 0;
        g_ephemeral_log_ctx.registry       = nullptr;
        g_ephemeral_log_ctx.config_manager = nullptr;
        g_ephemeral_initialized            = 1;
    }
}

AttachContext* bs_adapter_attach_ctx_create(void)
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

    ctx->owns_registry = 1;
    ctx->log_bus_bound = 0;
    if (bs_adapter_attach_ctx_init_config_manager(ctx) != 0)
    {
        bs_registry_facade_destroy(ctx->registry);
        std::free(ctx);
        return nullptr;
    }
    if (bs_adapter_attach_ctx_init_kernel(ctx) != 0)
    {
        bs_adapter_attach_ctx_destroy_config_manager(ctx);
        bs_registry_facade_destroy(ctx->registry);
        std::free(ctx);
        return nullptr;
    }
    bs_log_state_init(&ctx->log_state);
    bs_adapter_attach_session_init(ctx);
    ctx->kernel_pool        = bs_kernel_pool_create(nullptr);
    ctx->kernel_pool_warmed = 0;
    bs_adapter_attach_ir_snapshot_init(ctx);
    if (!ctx->kernel_pool)
    {
        bs_adapter_attach_session_destroy(ctx);
        bs_adapter_attach_ctx_teardown_kernel(ctx);
        bs_adapter_attach_ctx_destroy_config_manager(ctx);
        bs_registry_facade_destroy(ctx->registry);
        std::free(ctx);
        return nullptr;
    }
    return ctx;
}

void bs_adapter_attach_ctx_destroy(AttachContext* ctx)
{
    if (!ctx)
        return;

    pop_active_ctx_if_present(ctx);

    bs_adapter_attach_session_destroy(ctx);

    if (ctx->log_bus_bound)
    {
        bs_log_shutdown_bus_ctx(&ctx->log_state);
        ctx->log_bus_bound = 0;
        bs_adapter_attach_mark_log_ready(0);
    }

    if (ctx->owns_registry && ctx->registry)
        bs_registry_facade_destroy(ctx->registry);

    bs_adapter_attach_ctx_destroy_config_manager(ctx);
    bs_adapter_attach_ir_snapshot_destroy(ctx);
    if (ctx->kernel_pool)
    {
        bs_kernel_pool_destroy(ctx->kernel_pool);
        ctx->kernel_pool = nullptr;
    }
    bs_adapter_attach_ctx_teardown_kernel(ctx);

    std::memset(ctx, 0, sizeof(*ctx));
    std::free(ctx);
}

int bs_adapter_attach_ctx_init_kernel(AttachContext* ctx)
{
    if (!ctx)
        return -1;

    ctx->kernel                  = bs_kernel_create(nullptr);
    ctx->default_pipeline        = nullptr;
    ctx->kernel_started          = 0;
    ctx->pipeline_registry_bound = 0;
    if (!ctx->kernel)
        return -1;

    ctx->default_pipeline = bs_pipeline_create();
    if (!ctx->default_pipeline)
    {
        bs_kernel_destroy(ctx->kernel);
        ctx->kernel = nullptr;
        return -1;
    }

    Stage audit{};
    audit.name    = "audit";
    audit.execute = attach_audit_stage_execute;
    if (bs_pipeline_add_stage(ctx->default_pipeline, &audit) != 0)
    {
        bs_pipeline_destroy(ctx->default_pipeline);
        bs_kernel_destroy(ctx->kernel);
        ctx->default_pipeline = nullptr;
        ctx->kernel           = nullptr;
        return -1;
    }

    if (bs_kernel_register_pipeline(ctx->kernel, "default", ctx->default_pipeline) != 0)
    {
        bs_pipeline_destroy(ctx->default_pipeline);
        bs_kernel_destroy(ctx->kernel);
        ctx->default_pipeline = nullptr;
        ctx->kernel           = nullptr;
        return -1;
    }

    return 0;
}

void bs_adapter_attach_ctx_teardown_kernel(AttachContext* ctx)
{
    if (!ctx)
        return;

    bs_adapter_attach_ctx_stop_kernel(ctx);

    if (ctx->kernel && ctx->default_pipeline)
        bs_kernel_unregister_pipeline(ctx->kernel, "default");

    if (ctx->default_pipeline)
    {
        bs_pipeline_destroy(ctx->default_pipeline);
        ctx->default_pipeline = nullptr;
    }

    if (ctx->kernel)
    {
        bs_kernel_destroy(ctx->kernel);
        ctx->kernel = nullptr;
    }
}

Kernel* bs_adapter_attach_ctx_kernel(AttachContext* ctx)
{
    return ctx ? ctx->kernel : nullptr;
}

Pipeline* bs_adapter_attach_ctx_default_pipeline(AttachContext* ctx)
{
    return ctx ? ctx->default_pipeline : nullptr;
}

int bs_adapter_attach_ctx_bind_default_pipeline_registry(AttachContext* ctx, RegistryFacade* facade)
{
    if (!ctx || !facade || !ctx->default_pipeline)
        return -1;
    if (ctx->pipeline_registry_bound)
        return 0;

    PathEntry pipeline_entry{};
    pipeline_entry.source          = BS_PATH_ENTRY_BUILTIN;
    pipeline_entry.manifest_ref    = "builtin";
    pipeline_entry.type_constraint = "pipeline";

    if (bs_registry_facade_register_declaration(facade, "/kernel/pipeline/default",
                                                &pipeline_entry) != BS_REGISTRY_OK)
        return -1;

    if (bs_registry_facade_register_hub_mapping(facade, "kernel.pipeline.default",
                                                "/kernel/pipeline/default", 0) != BS_REGISTRY_OK)
        return -1;

    if (bs_registry_facade_bind_instance(facade, "/kernel/pipeline/default",
                                         ctx->default_pipeline) != BS_REGISTRY_OK)
        return -1;

    ctx->pipeline_registry_bound = 1;
    return 0;
}

int bs_adapter_attach_ctx_start_kernel(AttachContext* ctx)
{
    if (!ctx || !ctx->kernel)
        return -1;
    if (ctx->kernel_started)
        return 0;
    if (bs_kernel_start(ctx->kernel) != 0)
        return -1;
    ctx->kernel_started = 1;
    return 0;
}

void bs_adapter_attach_ctx_stop_kernel(AttachContext* ctx)
{
    if (!ctx || !ctx->kernel || !ctx->kernel_started)
        return;
    bs_kernel_stop(ctx->kernel);
    ctx->kernel_started = 0;
}

BsKernelPool* bs_adapter_attach_ctx_kernel_pool(AttachContext* ctx)
{
    return ctx ? ctx->kernel_pool : nullptr;
}

int bs_adapter_attach_ctx_warmup_kernel_pool(AttachContext* ctx)
{
    if (!ctx || !ctx->kernel_pool)
        return -1;
    if (ctx->kernel_pool_warmed)
        return 0;
    if (bs_kernel_pool_warmup(ctx->kernel_pool) != BS_KERNEL_POOL_OK)
        return -1;
    ctx->kernel_pool_warmed = 1;
    return 0;
}

int bs_adapter_attach_ctx_is_kernel_pool_warmed(const AttachContext* ctx)
{
    return ctx && ctx->kernel_pool_warmed;
}

int bs_adapter_attach_ctx_rebuild_kernel_pool(AttachContext* ctx)
{
    if (!ctx)
        return -1;
    if (ctx->kernel_pool)
    {
        bs_kernel_pool_destroy(ctx->kernel_pool);
        ctx->kernel_pool = nullptr;
    }
    ctx->kernel_pool_warmed = 0;
    ctx->kernel_pool        = bs_kernel_pool_create(nullptr);
    if (!ctx->kernel_pool)
        return -1;
    return bs_adapter_attach_ctx_warmup_kernel_pool(ctx);
}

RegistryFacade* bs_adapter_attach_ctx_registry(AttachContext* ctx)
{
    return ctx ? ctx->registry : nullptr;
}

ConfigManager* bs_adapter_attach_ctx_config_manager(AttachContext* ctx)
{
    return ctx ? ctx->config_manager : nullptr;
}

BsLogState* bs_adapter_attach_ctx_log_state(AttachContext* ctx)
{
    return ctx ? &ctx->log_state : nullptr;
}

int bs_adapter_attach_ctx_is_log_bus_bound(const AttachContext* ctx)
{
    return ctx && ctx->log_bus_bound != 0;
}

void bs_adapter_attach_ctx_set_log_bus_bound(AttachContext* ctx, int bound)
{
    if (!ctx)
        return;
    ctx->log_bus_bound = bound ? 1 : 0;
}

BsLogLevel bs_adapter_attach_ctx_get_log_level(const AttachContext* ctx)
{
    return ctx ? bs_log_get_global_level_ctx(&ctx->log_state) : BS_LOG_INFO;
}

void bs_adapter_attach_ctx_set_log_level(AttachContext* ctx, BsLogLevel level)
{
    if (!ctx)
        return;
    bs_log_set_global_level_ctx(&ctx->log_state, level);
}

void bs_adapter_attach_ctx_push_active(AttachContext* ctx)
{
    g_active_ctx_stack.push_back(ctx);
    sync_log_state_from_active();
}

void bs_adapter_attach_ctx_pop_active(AttachContext* ctx)
{
    pop_active_ctx_if_present(ctx);
}

void bs_adapter_attach_ctx_set_active(AttachContext* ctx)
{
    if (g_active_ctx_stack.empty())
        g_active_ctx_stack.push_back(ctx);
    else
        g_active_ctx_stack.back() = ctx;
    sync_log_state_from_active();
}

void bs_adapter_attach_ctx_active_access_enter(void)
{
    ++g_active_ctx_access_depth;
}

void bs_adapter_attach_ctx_active_access_leave(void)
{
    if (g_active_ctx_access_depth > 0)
        --g_active_ctx_access_depth;
}

AttachContext* bs_adapter_attach_ctx_get_active(void)
{
#ifndef NDEBUG
    assert(g_active_ctx_access_depth > 0 &&
           "g_active_ctx requires AttachActiveGuard (bs_adapter_attach_ctx_active_access_enter)");
#endif
    return active_ctx_top();
}

int bs_adapter_attach_ctx_is_active(const AttachContext* ctx)
{
    return ctx && active_ctx_top() == ctx;
}

int bs_adapter_attach_ctx_is_kernel_running(const AttachContext* ctx)
{
    if (!ctx || !ctx->kernel)
        return 0;
    return bs_kernel_get_state(ctx->kernel) == KERNEL_STATE_RUNNING ? 1 : 0;
}

void bs_adapter_attach_ensure_active_ctx(void)
{
    if (g_active_ctx_stack.empty())
    {
        ensure_ephemeral_initialized();
        bs_adapter_attach_ctx_push_active(&g_ephemeral_log_ctx);
    }
}

void bs_adapter_attach_ctx_use_external_registry(AttachContext* ctx, RegistryFacade* facade)
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

AttachContext* bs_adapter_attach_ctx_legacy_bootstrap(void)
{
    if (!g_legacy_initialized)
    {
        bs_log_state_init(&g_legacy_bootstrap_ctx.log_state);
        g_legacy_bootstrap_ctx.registry       = nullptr;
        g_legacy_bootstrap_ctx.config_manager = nullptr;
        g_legacy_bootstrap_ctx.owns_registry  = 0;
        g_legacy_bootstrap_ctx.log_bus_bound  = 0;
        g_legacy_initialized                  = 1;
    }
    return &g_legacy_bootstrap_ctx;
}

void bs_adapter_attach_ctx_shutdown_all_logs(void)
{
    /* Do not dereference TLS stack entries at process exit: tests may destroy ctx
     * without popping the stack (ASan heap-use-after-free in active_ctx_top). */
    g_active_ctx_stack.clear();
    sync_log_state_from_active();

    if (g_ephemeral_initialized && g_ephemeral_log_ctx.log_state.bus)
    {
        bs_log_shutdown_bus_ctx(&g_ephemeral_log_ctx.log_state);
        g_ephemeral_log_ctx.log_bus_bound = 0;
        g_ephemeral_initialized           = 0;
    }

    if (g_legacy_initialized &&
        (g_legacy_bootstrap_ctx.log_bus_bound || g_legacy_bootstrap_ctx.log_state.bus))
    {
        bs_log_shutdown_bus_ctx(&g_legacy_bootstrap_ctx.log_state);
        g_legacy_bootstrap_ctx.log_bus_bound = 0;
    }
}
