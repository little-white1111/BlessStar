#ifndef BS_KERNEL_COMMON_BS_WAIT_TRACE_H
#define BS_KERNEL_COMMON_BS_WAIT_TRACE_H

/*
 * C-ST-7 contract block:
 * Thread safety: env-mode cache read once; stderr logging only; no shared mutable state.
 * Error semantics: never throws; no-op when BS_WAIT_TRACE unset or off.
 * Platform notes: hang timing via GetTickCount64 (Windows) or clock_gettime (POSIX).
 *
 * Optional wait-chain tracing for attach flake diagnosis (session / pool / notify / persist I/O).
 *
 * BS_WAIT_TRACE=1       - log every instrumented wait site (verbose).
 * BS_WAIT_TRACE=hang    - log only after blocking >= BS_WAIT_TRACE_HANG_MS (default 3000).
 * BS_WAIT_TRACE_HANG_MS - hang threshold in milliseconds (default 3000).
 *
 * Instrumented sites (prefix):
 *   attach_session:*   session_mu / active_readers
 *   kernel_pool_*      pool reset/destroy busy_slots
 *   notify_queue:*     flush / watch_notify / worker join
 *   persist_io:*       manifest / fsync / sidecar
 */

#ifdef __cplusplus
extern "C"
{
#endif

    void bs_wait_trace(const char* site);
    void bs_wait_trace_u64(const char* site, unsigned long long ctx);
    void bs_wait_trace_path(const char* site, const char* path);

    int  bs_wait_trace_hang_begin(const char* site);
    void bs_wait_trace_hang_end(const char* site, int token);
    void bs_wait_trace_hang_tick(const char* site, int token);
    void bs_wait_trace_hang_tick_u64(const char* site, int token, unsigned long long ctx);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_COMMON_BS_WAIT_TRACE_H */
