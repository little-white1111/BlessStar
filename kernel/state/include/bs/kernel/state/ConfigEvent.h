#ifndef BS_KERNEL_STATE_CONFIGEVENT_H
#define BS_KERNEL_STATE_CONFIGEVENT_H

#include "ConfigState.h"

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#else
#include <stddef.h>
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum ConfigEventType
    {
        CONFIG_EVENT_ENTER_INITIAL,
        CONFIG_EVENT_ENTER_LOADING,
        CONFIG_EVENT_ENTER_ACTIVE,
        CONFIG_EVENT_ENTER_UPDATING,
        CONFIG_EVENT_ENTER_ERROR,
        CONFIG_EVENT_ENTER_CLOSED
    } ConfigEventType;

    typedef struct ConfigEvent
    {
        const char*     configPath;
        ConfigEventType type;
        ConfigState     fromState;
        ConfigState     toState;
        uint64_t        version;
        uint64_t        timestamp;
    } ConfigEvent;

    const char* ConfigEventType_ToString(ConfigEventType type);

    ConfigEvent* ConfigEvent_Create(const char* configPath, ConfigEventType type, ConfigState from,
                                    ConfigState to, uint64_t version);

    void ConfigEvent_Destroy(ConfigEvent* event);

#ifdef __cplusplus
}
#endif

#endif
