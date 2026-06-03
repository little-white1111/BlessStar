#ifndef BS_KERNEL_TEST_SUPPORT_BS_TEST_LOG_BUS_H
#define BS_KERNEL_TEST_SUPPORT_BS_TEST_LOG_BUS_H

/*
 * C-ST-7 contract block:
 * Thread safety: Test-only memory log bus; single-threaded tests only.
 * Error semantics: N/A for test harness binding.
 * Platform notes: Excluded from production gates; binds bs_log to capturing buffer.
 */

#include "bs/kernel/common/bs_log.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** Test-only memory sink (IMPL-08-07 / R8-03 B). No adapter headers. */
    int bs_test_log_bind_memory_bus(void (*on_line)(uint16_t domain_id, BsLogLevel level,
                                                    const char* line, void* ctx),
                                    void* ctx);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_TEST_SUPPORT_BS_TEST_LOG_BUS_H */
