#include "bs/kernel/common/bs_safe_format.h"
#include "bs/kernel/test_support/bs_test_log_bus.h"

#include <stdarg.h>
#include <stdio.h>

typedef struct TestLogMemCtx
{
    void (*on_line)(uint16_t domain_id, BsLogLevel level, const char* line, void* ctx);
    void* user;
} TestLogMemCtx;

static TestLogMemCtx g_test_log_mem;

static void test_memory_emit(uint16_t domain_id, BsLogLevel level, const char* fmt, va_list ap)
{
    char    buf[512];
    va_list copy;
    va_copy(copy, ap);
    bs_safe_vsnprintf(buf, sizeof(buf), fmt, copy);
    va_end(copy);
    if (g_test_log_mem.on_line)
        g_test_log_mem.on_line(domain_id, level, buf, g_test_log_mem.user);
}

static BsLogBusOps g_test_memory_ops = {test_memory_emit, NULL, NULL};

int bs_test_log_bind_memory_bus(void (*on_line)(uint16_t domain_id, BsLogLevel level,
                                                const char* line, void* ctx),
                                void* user_ctx)
{
    g_test_log_mem.on_line = on_line;
    g_test_log_mem.user    = user_ctx;
    return bs_log_bind_bus(&g_test_memory_ops);
}
