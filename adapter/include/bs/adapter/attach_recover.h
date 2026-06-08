#ifndef BS_ADAPTER_ATTACH_RECOVER_H
#define BS_ADAPTER_ATTACH_RECOVER_H

/*
 * C-ST-7 contract block:
 * Thread safety: recover_from_store and recover_cold_reload are driver-thread APIs.
 * Error semantics: Step-1 returns a RECOVERING AttachContext or NULL; Step-2 returns 0 on READY.
 * Platform notes: Explicit two-step crash recovery; no MVP auto_reload shortcut.
 */

#include "bs/kernel/io/io.h"
#include "bs/kernel/report/report.h"

#include "bs/adapter/attach_context.h"
#include "bs/adapter/orchestration/reload_batch_controller.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct BsAttachRecoverFromStoreOptions
    {
        size_t struct_size;
    } BsAttachRecoverFromStoreOptions;

    typedef struct BsAttachRecoverColdReloadOptions
    {
        size_t      struct_size;
        const char* manifest_path;
        IoFacade*   io_facade;

        ReloadPathReadFn read_fn;
        void*            read_ctx;
        ReloadPathGateFn gate_fn;
        void*            gate_ctx;

        BsAttachScheme scheme;
        unsigned       max_inflight;
        size_t         session_memory_cap;
        Report*        report;
    } BsAttachRecoverColdReloadOptions;

    AttachContext*
    bs_adapter_attach_recover_from_store(const char*                            manifest_path,
                                         const BsAttachRecoverFromStoreOptions* opts);

    int bs_adapter_attach_recover_cold_reload(AttachContext*                          ctx,
                                              const BsAttachRecoverColdReloadOptions* opts);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_ATTACH_RECOVER_H */
