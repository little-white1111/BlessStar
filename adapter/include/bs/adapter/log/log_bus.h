#ifndef BS_ADAPTER_LOG_LOG_BUS_H
#define BS_ADAPTER_LOG_LOG_BUS_H

/*
 * C-ST-7 contract block:
 * Thread safety: Depends on backend (memory vs spdlog); see adapter_log implementation.
 * Error semantics: Never throws across C API; drops if domain unknown.
 * Platform notes: Binds kernel bs_log domains to adapter sinks.
 */

#include "bs/kernel/common/bs_log.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** Bind spdlog-backed single sink (LOG-VII-1). Returns 0 on success. */
    int bs_adapter_log_bind_spdlog_bus(void);

    /** Test sink: captures formatted lines (no spdlog required). */
    int bs_adapter_log_bind_memory_bus(void (*on_line)(uint16_t domain_id, BsLogLevel level,
                                                       const char* line, void* ctx),
                                       void* ctx);

    /** Flush/shutdown active log bus (CI LSan + attach destroy). */
    void bs_adapter_log_shutdown_if_bound(void);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_LOG_LOG_BUS_H */
