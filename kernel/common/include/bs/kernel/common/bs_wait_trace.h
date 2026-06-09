#ifndef BS_KERNEL_COMMON_BS_WAIT_TRACE_H
#define BS_KERNEL_COMMON_BS_WAIT_TRACE_H

/*
 * Optional wait-chain tracing for pool / executor hang diagnosis.
 *
 * BS_WAIT_TRACE=1       - log every instrumented wait site (verbose).
 * BS_WAIT_TRACE=hang    - log only after blocking >= BS_WAIT_TRACE_HANG_MS (default 3000).
 * BS_WAIT_TRACE_HANG_MS - hang threshold in milliseconds (default 3000).
 */

#ifdef __cplusplus
extern "C"
{
#endif

    void bs_wait_trace(const char* site);
    void bs_wait_trace_u64(const char* site, unsigned long long ctx);

    int  bs_wait_trace_hang_begin(const char* site);
    void bs_wait_trace_hang_tick(const char* site, int token);
    void bs_wait_trace_hang_tick_u64(const char* site, int token, unsigned long long ctx);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_COMMON_BS_WAIT_TRACE_H */
