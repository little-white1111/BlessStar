#include "bs/kernel/state/ConfigEvent.h"

#include <cstdlib>
#include <cstring>
#include <ctime>

static const char* eventTypeNames[] = {"ENTER_INITIAL",  "ENTER_LOADING", "ENTER_ACTIVE",
                                       "ENTER_UPDATING", "ENTER_ERROR",   "ENTER_CLOSED"};

const char* ConfigEventType_ToString(ConfigEventType type)
{
    if (type < 0 || type >= sizeof(eventTypeNames) / sizeof(eventTypeNames[0]))
    {
        return "UNKNOWN";
    }
    return eventTypeNames[type];
}

ConfigEvent* ConfigEvent_Create(const char* configPath, ConfigEventType type, ConfigState from,
                                ConfigState to, uint64_t version)
{
    ConfigEvent* event = (ConfigEvent*)malloc(sizeof(ConfigEvent));
    if (!event)
        return nullptr;

    event->configPath = configPath ? strdup(configPath) : nullptr;
    event->type       = type;
    event->fromState  = from;
    event->toState    = to;
    event->version    = version;
    event->timestamp  = (uint64_t)time(nullptr);

    return event;
}

void ConfigEvent_Destroy(ConfigEvent* event)
{
    if (event)
    {
        if (event->configPath)
        {
            free((void*)event->configPath);
        }
        free(event);
    }
}
