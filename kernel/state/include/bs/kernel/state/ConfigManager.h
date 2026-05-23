#ifndef BS_KERNEL_STATE_CONFIGMANAGER_H
#define BS_KERNEL_STATE_CONFIGMANAGER_H

#include "ConfigEvent.h"
#include "ConfigState.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void (*WatchCallback)(const char* path, ConfigEventType type, const void* data,
                                  void* userData);

    typedef struct StateBus       StateBus;
    typedef struct EventBus       EventBus;
    typedef struct StateMachine   StateMachine;
    typedef struct TemporaryState TemporaryState;
    typedef struct WatchManager   WatchManager;

    typedef struct ConfigManager ConfigManager;

    ConfigManager* ConfigManager_Create();

    void ConfigManager_Destroy(ConfigManager* cm);

    int ConfigManager_LoadConfig(ConfigManager* cm, const char* path, const void* data,
                                 size_t dataSize);

    int ConfigManager_ReloadConfig(ConfigManager* cm, const char* path, const void* data,
                                   size_t dataSize);

    int ConfigManager_UnloadConfig(ConfigManager* cm, const char* path);

    int ConfigManager_HotUpdate(ConfigManager* cm, const char* path, const void* newData,
                                size_t newDataSize);

    int ConfigManager_GetConfigState(ConfigManager* cm, const char* path, ConfigState* state);

    int ConfigManager_GetConfigSnapshot(ConfigManager* cm, const char* path, void** data,
                                        size_t* size);

    int ConfigManager_SubscribeStateChange(ConfigManager* cm, const char* path,
                                           WatchCallback callback, void* userData);

    int ConfigManager_UnsubscribeStateChange(ConfigManager* cm, const char* path,
                                             WatchCallback callback);

    StateBus* ConfigManager_GetStateBus(ConfigManager* cm);

    EventBus* ConfigManager_GetEventBus(ConfigManager* cm);

    WatchManager* ConfigManager_GetWatchManager(ConfigManager* cm);

#ifdef __cplusplus
}
#endif

#endif
