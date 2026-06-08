#ifndef BS_ADAPTER_ATTACH_RECOVER_SIDECAR_H
#define BS_ADAPTER_ATTACH_RECOVER_SIDECAR_H

/*
 * C-ST-7 contract block:
 * Thread safety: sidecar APIs are driver-thread; invalidate on reload batch start.
 * Error semantics: fast hydrate returns BS_ATTACH_* / -1; invalid sidecar degrades to cold reload.
 * Platform notes: Layer-C optional; default off via BS_ATTACH_RECOVER_SIDECAR env flag.
 */

/*
 * Layer-C runtime sidecar (REC-A'-11..14 / T-REC.11..13).
 * Default off; enable with BS_ATTACH_RECOVER_SIDECAR=1.
 * Crash paths never trust sidecar alone; validate then degrade to cold reload.
 */

#include "bs/adapter/attach_context.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** 1 when env BS_ATTACH_RECOVER_SIDECAR=1 (or BS_TESTING override). */
    int bs_adapter_attach_recover_sidecar_enabled(void);

    int bs_adapter_attach_recover_sidecar_invalidate(const char* manifest_path);

    /** Post-flip READY write with CLEAN_SHUTDOWN flag (REC-A'-11). */
    int bs_adapter_attach_recover_sidecar_write_ready(AttachContext* ctx,
                                                      const char*    manifest_path);

    /** Hydrate ConfigManager from manifest canonical files (no IoFacade read). */
    int bs_adapter_attach_recover_fast_hydrate(AttachContext* ctx, const char* manifest_path);

    /** 1 when sidecar feature enabled and on-disk ckpt matches manifest digest. */
    int bs_adapter_attach_recover_sidecar_can_fast_hydrate(const char* manifest_path);

#if defined(BS_TESTING)
    void bs_adapter_attach_recover_sidecar_testing_set_enabled(int enabled);
#endif

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_ATTACH_RECOVER_SIDECAR_H */
