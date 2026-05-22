#include "bs/kernel/common/bs_log.h"
#include "bs/kernel/test_support/bs_test_log_bus.h"

#include <cassert>
#include <cstring>

static int g_audit_seen  = 0;
static int g_debug_seen  = 0;

static void capture(uint16_t, BsLogLevel level, const char* line, void*)
{
    if (!line)
        return;
    if (level == BS_LOG_AUDIT)
        ++g_audit_seen;
    if (level == BS_LOG_DEBUG && std::strstr(line, "debug-line"))
        ++g_debug_seen;
}

int main()
{
    assert(bs_test_log_bind_memory_bus(capture, nullptr) == 0);
    bs_log_set_global_level(BS_LOG_ERROR);

    bs_log_emit(1, BS_LOG_DEBUG, "debug-line");
    bs_log_emit(1, BS_LOG_AUDIT, "audit-line");
    bs_log_flush();

    assert(g_audit_seen == 1);
    assert(g_debug_seen == 0);
    return 0;
}
