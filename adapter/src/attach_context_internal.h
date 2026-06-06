#ifndef BS_ADAPTER_ATTACH_CONTEXT_INTERNAL_H
#define BS_ADAPTER_ATTACH_CONTEXT_INTERNAL_H

/*
 * Internal header (not scanned by C-ST-7 public gate).
 * C-ST-7 contract block (internal):
 * Thread safety: One AttachContext per attach session on the driver thread; active ctx via
 *   bs_adapter_attach_ctx_set_active for orchestration/reload.
 * Error semantics: init/teardown return -1 on allocation or registry bind failure.
 * Platform notes: registry + config_manager + kernel + default_pipeline (XVII-ATTACH-1 /
 *   XVII-KERNEL-1 / XVII-CM-1). PER_BATCH CM checkpoint/rollback uses active ctx only;
 *   shells without config_manager skip CM sync.
 */

#include "bs/kernel/common/bs_log.h"
#include "bs/kernel/registry/registry_facade.h"
#include "bs/kernel/runtime/Kernel.h"
#include "bs/kernel/runtime/kernel_pool.h"
#include "bs/kernel/state/ConfigEvent.h"
#include "bs/kernel/state/WatchManager.h"

#include "bs/adapter/attach_context.h"

struct Pipeline;

struct AttachContext
{
    RegistryFacade* registry;
    ConfigManager*  config_manager;
    Kernel*         kernel;
    Pipeline*       default_pipeline;
    BsLogState      log_state;
    int             log_bus_bound;
    int             owns_registry;
    int             kernel_started;
    int             pipeline_registry_bound;
    void*           session_state; /* AttachSessionState (attach_session.cpp) */
    void*           notify_queue;  /* AttachNotifyQueue (attach_notify_queue.cpp) */
    BsKernelPool*   kernel_pool;
    int             kernel_pool_warmed;
    void*           ir_snapshot_store; /* IrSnapshotStore (attach_ir_snapshot.cpp) */
};

int  bs_adapter_attach_ctx_init_config_manager(AttachContext* ctx);
void bs_adapter_attach_ctx_destroy_config_manager(AttachContext* ctx);

int  bs_adapter_attach_ctx_init_kernel(AttachContext* ctx);
void bs_adapter_attach_ctx_teardown_kernel(AttachContext* ctx);

Kernel*   bs_adapter_attach_ctx_kernel(AttachContext* ctx);
Pipeline* bs_adapter_attach_ctx_default_pipeline(AttachContext* ctx);

int  bs_adapter_attach_ctx_bind_default_pipeline_registry(AttachContext*  ctx,
                                                          RegistryFacade* facade);
int  bs_adapter_attach_ctx_start_kernel(AttachContext* ctx);
void bs_adapter_attach_ctx_stop_kernel(AttachContext* ctx);

BsKernelPool* bs_adapter_attach_ctx_kernel_pool(AttachContext* ctx);
int           bs_adapter_attach_ctx_warmup_kernel_pool(AttachContext* ctx);
int           bs_adapter_attach_ctx_is_kernel_pool_warmed(const AttachContext* ctx);

typedef void (*BsAttachPhase2NotifyBridgeFn)(ConfigManager* cm, WatchManager* wm,
                                             const char* path, ConfigEventType type,
                                             const void* snapshot, void* user_data);

void bs_adapter_attach_config_register_phase2_notify(AttachContext* ctx,
                                                   BsAttachPhase2NotifyBridgeFn fn);

void bs_adapter_attach_config_clear_phase2_notify(AttachContext* ctx);

/** Caller must hold attach session read lock. */
int bs_adapter_attach_config_snapshot_bytes_locked(AttachContext* ctx, const char* config_path,
                                                   size_t* total_out, void** bytes_out,
                                                   size_t* bytes_len_out);

#endif
