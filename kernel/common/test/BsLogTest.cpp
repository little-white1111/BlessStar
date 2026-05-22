#include "bs/kernel/common/bs_log.h"
#include "bs/kernel/test_support/bs_test_log_bus.h"

#include <cassert>
#include <cstring>

static int g_line_count = 0;

static void capture_line(uint16_t, BsLogLevel, const char* line, void*)
{
    if (line && std::strstr(line, "pre-bind"))
        ++g_line_count;
}

int main()
{
    bs_log_emit(1, BS_LOG_INFO, "pre-bind %d", 1);
    assert(bs_test_log_bind_memory_bus(capture_line, nullptr) == 0);
    assert(g_line_count >= 1);

    bs_log_emit(1, BS_LOG_DEBUG, "after-bind");
    bs_log_flush();
    return 0;
}
