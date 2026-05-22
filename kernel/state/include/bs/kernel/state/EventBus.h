#ifndef BS_KERNEL_STATE_EVENTBUS_H
#define BS_KERNEL_STATE_EVENTBUS_H

#include "ConfigEvent.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void (*EventListener)(const ConfigEvent* event, void* userData);

    typedef struct EventBus EventBus;

    EventBus* EventBus_Create();

    void EventBus_Destroy(EventBus* bus);

    int EventBus_Subscribe(EventBus* bus, const char* path, EventListener listener, void* userData);

    int EventBus_Unsubscribe(EventBus* bus, const char* path, EventListener listener);

    /** Enqueue only; delivery happens in EventBus_Drain (IMPL-06-04). */
    int EventBus_Publish(EventBus* bus, const ConfigEvent* event);

    int EventBus_Drain(EventBus* bus);

#ifdef __cplusplus
}
#endif

#endif
