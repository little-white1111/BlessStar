#ifndef BS_KERNEL_STATE_WATCHMANAGER_H
#define BS_KERNEL_STATE_WATCHMANAGER_H

#include "ConfigEvent.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum WatchMode
    {
        WATCH_MODE_ONCE,
        WATCH_MODE_PERSISTENT,
        WATCH_MODE_PERSISTENT_RECURSIVE
    } WatchMode;

    typedef struct Watcher Watcher;

    typedef void (*WatchCallback)(const char* path, ConfigEventType type, const void* data,
                                  void* userData);

    typedef struct WatchManager WatchManager;

    WatchManager* WatchManager_Create();

    void WatchManager_Destroy(WatchManager* wm);

    int WatchManager_AddWatch(WatchManager* wm, const char* path, WatchCallback callback,
                              WatchMode mode, void* userData);

    int WatchManager_RemoveWatch(WatchManager* wm, const char* path, WatchCallback callback);

    int WatchManager_Notify(WatchManager* wm, const char* path, ConfigEventType type,
                            const void* data);

#ifdef __cplusplus
}
#endif

#endif
