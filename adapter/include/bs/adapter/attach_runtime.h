#ifndef BS_ADAPTER_ATTACH_RUNTIME_H
#define BS_ADAPTER_ATTACH_RUNTIME_H

/*
 * C-ST-7 contract block:
 * Thread safety: Log-ready flag is read/written on the attach driver thread only.
 * Error semantics: is_log_ready returns 0/1; mark_log_ready is void (no failure path).
 * Platform notes: Tracks whether bootstrap bound the log bus on the active AttachContext
 *   (LOG-VII-10). Reload entry (bs_adapter_attach_reload_batch_run) requires is_log_ready before
 * I/O.
 */

#ifdef __cplusplus
extern "C"
{
#endif

    /** True after successful bootstrap bind_log_bus (LOG-VII-10). */
    int bs_adapter_attach_is_log_ready(void);

    void bs_adapter_attach_mark_log_ready(int ready);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_ATTACH_RUNTIME_H */
