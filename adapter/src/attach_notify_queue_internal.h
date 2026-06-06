#ifndef BS_ADAPTER_ATTACH_NOTIFY_QUEUE_INTERNAL_H
#define BS_ADAPTER_ATTACH_NOTIFY_QUEUE_INTERNAL_H

#include "bs/kernel/state/ConfigEvent.h"
#include "bs/kernel/state/WatchManager.h"

#include "attach_context_internal.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void bs_adapter_attach_notify_queue_bind(AttachContext* ctx);
    void bs_adapter_attach_notify_queue_flush(AttachContext* ctx);
    void bs_adapter_attach_notify_queue_shutdown(AttachContext* ctx);

    void bs_adapter_attach_notify_queue_enqueue_watch(AttachContext* ctx, WatchManager* wm,
                                                      const char* path, ConfigEventType type,
                                                      const void* snapshot);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_ATTACH_NOTIFY_QUEUE_INTERNAL_H */
