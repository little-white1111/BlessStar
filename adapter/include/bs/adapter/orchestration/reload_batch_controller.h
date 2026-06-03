#ifndef BS_ADAPTER_ORCHESTRATION_RELOAD_BATCH_CONTROLLER_H
#define BS_ADAPTER_ORCHESTRATION_RELOAD_BATCH_CONTROLLER_H

#include "bs/kernel/io/io.h"

#include "bs/adapter/persistence/attach_store.h"

struct Report;

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum BatchOutcome
    {
        BATCH_ALL_OK                  = 0,
        BATCH_COMPLETED_WITH_FAILURES = 1
    } BatchOutcome;

    typedef enum PathOrchestrationState
    {
        BS_ORCH_PENDING          = 0,
        BS_ORCH_READING          = 1,
        BS_ORCH_GATING           = 2,
        BS_ORCH_COMMITTED        = 3,
        BS_ORCH_GATE_REJECTED    = 4,
        BS_ORCH_FAILED_READ      = 5,
        BS_ORCH_PERSIST_REJECTED = 6,
        BS_ORCH_STAGED           = 7
    } PathOrchestrationState;

#define BS_ATTACH_SESSION_MEMORY_CAP_DEFAULT (16u * 1024u * 1024u)

    typedef struct ReloadBatchController ReloadBatchController;

    typedef int (*ReloadPathReadFn)(void* user_ctx, const char* uri, IoReadResult* out);

    typedef struct BsReloadGateDetail
    {
        char buf[256];
    } BsReloadGateDetail;

    typedef int (*ReloadPathGateFn)(void* user_ctx, const char* uri,
                                    const IoReadResult* read_result,
                                    BsReloadGateDetail* detail_out);

    ReloadBatchController* bs_adapter_attach_reload_batch_create(unsigned max_inflight);
    void                   bs_adapter_attach_reload_batch_destroy(ReloadBatchController* ctrl);

    void bs_adapter_attach_reload_batch_set_read_fn(ReloadBatchController* ctrl,
                                                    ReloadPathReadFn fn, void* user_ctx);

    void bs_adapter_attach_reload_batch_set_gate_fn(ReloadBatchController* ctrl,
                                                    ReloadPathGateFn fn, void* user_ctx);

    void bs_adapter_attach_reload_batch_set_default_gate(ReloadBatchController* ctrl);

    void bs_adapter_attach_reload_batch_set_max_retry(ReloadBatchController* ctrl,
                                                      unsigned               max_retry);

    void bs_adapter_attach_reload_batch_set_report(ReloadBatchController* ctrl,
                                                   struct Report*         report);

    int bs_adapter_attach_reload_batch_set_attach_scheme(ReloadBatchController* ctrl,
                                                         BsAttachScheme         scheme);

    void bs_adapter_attach_reload_batch_set_manifest_path(ReloadBatchController* ctrl,
                                                          const char*            manifest_path);

    void bs_adapter_attach_reload_batch_set_session_memory_cap(ReloadBatchController* ctrl,
                                                               size_t                 cap_bytes);

    int bs_adapter_attach_reload_batch_add_path(ReloadBatchController* ctrl, const char* uri);

    int bs_adapter_attach_reload_batch_run(ReloadBatchController* ctrl);

    BatchOutcome bs_adapter_attach_reload_batch_outcome(const ReloadBatchController* ctrl);

    PathOrchestrationState
    bs_adapter_attach_reload_batch_path_state(const ReloadBatchController* ctrl, const char* uri);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_ORCHESTRATION_RELOAD_BATCH_CONTROLLER_H */
