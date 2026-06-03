#ifndef BS_KERNEL_STATE_EVENTBUS_H
#define BS_KERNEL_STATE_EVENTBUS_H

/*
 * C-ST-7 contract block:
 * Thread safety: Internally synchronized; listeners invoked from bs_event_bus_drain only.
 * Error semantics: 0 ok; -1 invalid; -2 unsubscribe miss; publish queues until drain.
 * Platform notes: Reentrancy guards wrap listener callbacks (bs_reentrancy_*).
 */

#include "ConfigEvent.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void (*EventListener)(const ConfigEvent* event, void* userData);

    typedef struct EventBus EventBus;

    EventBus* bs_event_bus_create();

    void bs_event_bus_destroy(EventBus* bus);

    int bs_event_bus_subscribe(EventBus* bus, const char* path, EventListener listener,
                               void* userData);

    int bs_event_bus_unsubscribe(EventBus* bus, const char* path, EventListener listener);

    /** Enqueue only; delivery happens in bs_event_bus_drain (IMPL-06-04). */
    int bs_event_bus_publish(EventBus* bus, const ConfigEvent* event);

    int bs_event_bus_drain(EventBus* bus);

#ifdef __cplusplus
}
#endif

#endif
