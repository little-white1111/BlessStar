#ifndef BS_KERNEL_STATE_WATCHMANAGER_H
#define BS_KERNEL_STATE_WATCHMANAGER_H

/*
 * C-ST-7 contract block:
 * Thread safety: Internally synchronized; callbacks run on notify thread without reentrancy guard.
 * Error semantics: 0 ok; -1 invalid; -2 remove miss; WATCH_MODE_ONCE auto-removes after fire.
 * Platform notes: Path-keyed watcher lists; pairs with ConfigManager state notifications.
 */

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

    WatchManager* bs_watch_manager_create();

    void bs_watch_manager_destroy(WatchManager* wm);

    int bs_watch_manager_add_watch(WatchManager* wm, const char* path, WatchCallback callback,
                                   WatchMode mode, void* userData);

    int bs_watch_manager_remove_watch(WatchManager* wm, const char* path, WatchCallback callback);

    int bs_watch_manager_notify(WatchManager* wm, const char* path, ConfigEventType type,
                                const void* data);

#ifdef __cplusplus
}
#endif

#endif
