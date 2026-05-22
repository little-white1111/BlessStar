#ifndef BS_KERNEL_COMMON_BS_LOG_H
#define BS_KERNEL_COMMON_BS_LOG_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum BsLogLevel
    {
        BS_LOG_TRACE = 0,
        BS_LOG_DEBUG = 1,
        BS_LOG_INFO  = 2,
        BS_LOG_WARN  = 3,
        BS_LOG_ERROR = 4,
        BS_LOG_AUDIT = 5,
        BS_LOG_OFF   = 6
    } BsLogLevel;

    typedef struct BsLogBusOps
    {
        void (*emit)(uint16_t domain_id, BsLogLevel level, const char* fmt, va_list ap);
        void (*flush)(void);
        void (*shutdown)(void);
    } BsLogBusOps;

#define BS_LOG_RING_CAPACITY 64

    typedef struct BsLogRingEntry
    {
        uint16_t   domain_id;
        BsLogLevel level;
        char       message[256];
    } BsLogRingEntry;

    /** Per-attach log bus + ring buffer (R8-02 · IMPL-08-06 phase 3). */
    typedef struct BsLogState
    {
        const BsLogBusOps* bus;
        BsLogLevel         global_level;
        BsLogRingEntry     ring[BS_LOG_RING_CAPACITY];
        unsigned           ring_head;
        unsigned           ring_count;
    } BsLogState;

    void bs_log_state_init(BsLogState* state);

    int bs_log_bind_bus_ctx(BsLogState* ctx, const BsLogBusOps* ops);

    int bs_log_bus_is_bound_ctx(const BsLogState* ctx);

    void bs_log_set_global_level_ctx(BsLogState* ctx, BsLogLevel level);

    BsLogLevel bs_log_get_global_level_ctx(const BsLogState* ctx);

    void bs_log_emit_ctx(BsLogState* ctx, uint16_t domain_id, BsLogLevel level, const char* fmt,
                         ...);

    void bs_log_flush_ctx(BsLogState* ctx);

    /** Active state for legacy wrappers; set by attach on `bs_attach_context_set_active`. */
    void       bs_log_set_current_state(BsLogState* state);
    BsLogState* bs_log_get_current_state(void);

    /** Legacy wrappers: operate on current state (fallback when no active attach). */
    int bs_log_bind_bus(const BsLogBusOps* ops);

    int bs_log_bus_is_bound(void);

    void bs_log_set_global_level(BsLogLevel level);

    BsLogLevel bs_log_get_global_level(void);

    void bs_log_emit(uint16_t domain_id, BsLogLevel level, const char* fmt, ...);

    void bs_log_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_COMMON_BS_LOG_H */
