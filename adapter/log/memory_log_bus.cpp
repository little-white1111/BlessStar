#include "bs/kernel/common/bs_safe_format.h"

#include "bs/adapter/attach_context.h"
#include "bs/adapter/attach_runtime.h"
#include "bs/adapter/log/log_bus.h"

#include <cstdarg>

struct MemoryBusCtx
{
    void (*on_line)(uint16_t, BsLogLevel, const char*, void*);
    void* user;
};

static MemoryBusCtx g_mem_ctx{};

static void memory_emit(uint16_t domain_id, BsLogLevel level, const char* fmt, va_list ap)
{
    char    buf[512];
    va_list copy;
    va_copy(copy, ap);
    bs_safe_vsnprintf(buf, sizeof(buf), fmt, copy);
    va_end(copy);
    if (g_mem_ctx.on_line)
        g_mem_ctx.on_line(domain_id, level, buf, g_mem_ctx.user);
}

static BsLogBusOps g_memory_ops = {memory_emit, nullptr, nullptr};

int bs_adapter_log_bind_memory_bus(void (*on_line)(uint16_t domain_id, BsLogLevel level,
                                                   const char* line, void* ctx),
                                   void* ctx)
{
    g_mem_ctx.on_line = on_line;
    g_mem_ctx.user    = ctx;
    const int rc      = bs_log_bind_bus(&g_memory_ops);
    if (rc == 0)
    {
        bs_adapter_attach_ensure_active_ctx();
        bs_adapter_attach_mark_log_ready(1);
    }
    return rc;
}

#if defined(BLESSSTAR_SANITIZER_CI)

static void sanitizer_spdlog_stub_log(uint16_t domain_id, BsLogLevel level, const char* line,
                                      void* ctx)
{
    (void)domain_id;
    (void)level;
    (void)line;
    (void)ctx;
}

int bs_adapter_log_bind_spdlog_bus(void)
{
    return bs_adapter_log_bind_memory_bus(sanitizer_spdlog_stub_log, nullptr);
}

void bs_adapter_log_shutdown_if_bound(void)
{
    bs_adapter_attach_ctx_shutdown_all_logs();
    bs_adapter_attach_mark_log_ready(0);
}

#endif /* BLESSSTAR_SANITIZER_CI */
