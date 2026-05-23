#ifndef BS_ADAPTER_LOG_LOG_BUS_H
#define BS_ADAPTER_LOG_LOG_BUS_H

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

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_LOG_LOG_BUS_H */
