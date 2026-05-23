#ifndef BS_KERNEL_TEST_SUPPORT_BS_TEST_LOG_BUS_H
#define BS_KERNEL_TEST_SUPPORT_BS_TEST_LOG_BUS_H

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
