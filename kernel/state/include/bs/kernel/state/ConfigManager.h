#ifndef BS_KERNEL_STATE_CONFIGMANAGER_H
#define BS_KERNEL_STATE_CONFIGMANAGER_H

/*
 * C-ST-7 contract block:
 * Thread safety: Not thread-safe; external lock if shared across threads.
 * Error semantics: 0 success; -1 invalid arg; -2 not found; -3 path already loaded (load).
 * Platform notes: Coordinates StateBus + EventBus drain + WatchManager notify on transitions.
 */

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

    ConfigManager* bs_config_manager_create();

    void bs_config_manager_destroy(ConfigManager* cm);

    int bs_config_manager_load_config(ConfigManager* cm, const char* path, const void* data,
                                      size_t dataSize);

    int bs_config_manager_reload_config(ConfigManager* cm, const char* path, const void* data,
                                        size_t dataSize);

    int bs_config_manager_unload_config(ConfigManager* cm, const char* path);

    int bs_config_manager_hot_update(ConfigManager* cm, const char* path, const void* newData,
                                     size_t newDataSize);

    int bs_config_manager_get_config_state(ConfigManager* cm, const char* path, ConfigState* state);

    int bs_config_manager_get_config_snapshot(ConfigManager* cm, const char* path, void** data,
                                              size_t* size);

    int bs_config_manager_subscribe_state_change(ConfigManager* cm, const char* path,
                                                 WatchCallback callback, void* userData);

    int bs_config_manager_unsubscribe_state_change(ConfigManager* cm, const char* path,
                                                   WatchCallback callback);

    StateBus* bs_config_manager_get_state_bus(ConfigManager* cm);

    EventBus* bs_config_manager_get_event_bus(ConfigManager* cm);

    WatchManager* bs_config_manager_get_watch_manager(ConfigManager* cm);

    /** Optional ordered phase-2 watch notify (T20.6); NULL hook keeps synchronous notify. */
    typedef void (*BsConfigManagerPhase2NotifyFn)(ConfigManager* cm, WatchManager* wm,
                                                  const char* path, ConfigEventType type,
                                                  const void* snapshot, size_t snapshot_size,
                                                  void* user_data);

    void bs_config_manager_set_phase2_notify_hook(ConfigManager*                cm,
                                                  BsConfigManagerPhase2NotifyFn fn,
                                                  void*                         user_data);

#ifdef __cplusplus
}
#endif

#endif
