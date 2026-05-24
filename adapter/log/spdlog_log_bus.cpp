#include "bs/kernel/common/bs_safe_format.h"

#include "bs/adapter/attach_context.h"
#include "bs/adapter/attach_runtime.h"
#include "bs/adapter/log/log_bus.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include <memory>
#include <mutex>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

static std::shared_ptr<spdlog::logger> g_logger;
static std::recursive_mutex            g_log_mutex;
static int                             g_spdlog_lib_shutdown = 0;

static spdlog::level::level_enum map_level(BsLogLevel level)
{
    switch (level)
    {
    case BS_LOG_TRACE:
        return spdlog::level::trace;
    case BS_LOG_DEBUG:
        return spdlog::level::debug;
    case BS_LOG_INFO:
        return spdlog::level::info;
    case BS_LOG_WARN:
        return spdlog::level::warn;
    case BS_LOG_ERROR:
        return spdlog::level::err;
    case BS_LOG_AUDIT:
        return spdlog::level::critical;
    default:
        return spdlog::level::off;
    }
}

static void spdlog_emit(uint16_t domain_id, BsLogLevel level, const char* fmt, va_list ap)
{
    char    buf[512];
    va_list copy;
    va_copy(copy, ap);
    bs_safe_vsnprintf(buf, sizeof(buf), fmt, copy);
    va_end(copy);

    std::lock_guard<std::recursive_mutex> lock(g_log_mutex);
    if (!g_logger)
        return;
    g_logger->log(map_level(level), "[d{}] {}", domain_id, buf);
}

static void spdlog_flush(void)
{
    std::lock_guard<std::recursive_mutex> lock(g_log_mutex);
    if (g_logger)
        g_logger->flush();
}

static void spdlog_shutdown(void)
{
    std::lock_guard<std::recursive_mutex> lock(g_log_mutex);
    if (g_logger)
    {
        g_logger->flush();
        spdlog::drop(g_logger->name());
        g_logger.reset();
    }
    if (!g_spdlog_lib_shutdown)
    {
        spdlog::shutdown();
        g_spdlog_lib_shutdown = 1;
    }
}

static void blessstar_log_atexit_cleanup(void)
{
    bs_adapter_log_shutdown_if_bound();
}

static int g_log_atexit_registered = 0;

static BsLogBusOps g_spdlog_ops = {spdlog_emit, spdlog_flush, spdlog_shutdown};

int bs_adapter_log_bind_spdlog_bus(void)
{
    std::lock_guard<std::recursive_mutex> lock(g_log_mutex);
    if (!g_logger)
    {
        if (g_spdlog_lib_shutdown)
            g_spdlog_lib_shutdown = 0;
        auto sink = std::make_shared<spdlog::sinks::null_sink_mt>();
        g_logger  = std::make_shared<spdlog::logger>("blessstar", sink);
        g_logger->set_level(spdlog::level::trace);
    }
    const int rc = bs_log_bind_bus(&g_spdlog_ops);
    if (rc == 0)
    {
        bs_adapter_attach_ensure_active_ctx();
        bs_adapter_attach_mark_log_ready(1);
        if (!g_log_atexit_registered)
        {
            g_log_atexit_registered = 1;
            std::atexit(blessstar_log_atexit_cleanup);
        }
    }
    return rc;
}

void bs_adapter_log_shutdown_if_bound(void)
{
    bs_attach_context_shutdown_all_logs();

    {
        std::lock_guard<std::recursive_mutex> lock(g_log_mutex);
        if (g_logger)
            spdlog_shutdown();
    }

    bs_adapter_attach_mark_log_ready(0);
}

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
