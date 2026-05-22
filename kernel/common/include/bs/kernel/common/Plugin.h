#ifndef BS_KERNEL_COMMON_PLUGIN_H
#define BS_KERNEL_COMMON_PLUGIN_H

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
extern "C"
{
#else
#include <stddef.h>
#include <stdint.h>
#endif

    typedef enum PluginType
    {
        PLUGIN_TYPE_FORMAT_PARSER = 1,
        PLUGIN_TYPE_SCHEMA_LOADER = 2,
        PLUGIN_TYPE_VALIDATOR     = 3,
        PLUGIN_TYPE_IR_GENERATOR  = 4,
        PLUGIN_TYPE_EXECUTOR      = 5,
        PLUGIN_TYPE_OBSERVER      = 6
    } PluginType;

    typedef enum PluginState
    {
        PLUGIN_STATE_UNLOADED = 0,
        PLUGIN_STATE_LOADED   = 1,
        PLUGIN_STATE_ACTIVE   = 2,
        PLUGIN_STATE_ERROR    = 3
    } PluginState;

    typedef struct Plugin        Plugin;
    typedef struct PluginManager PluginManager;

    typedef int (*PluginInitFunc)(Plugin* plugin);
    typedef int (*PluginDestroyFunc)(Plugin* plugin);
    typedef int (*PluginStartFunc)(Plugin* plugin);
    typedef int (*PluginStopFunc)(Plugin* plugin);
    typedef const char* (*PluginGetInfoFunc)(Plugin* plugin, const char* key);

    struct Plugin
    {
        const char* name;
        const char* version;
        PluginType  type;
        PluginState state;
        void*       handle;

        PluginInitFunc    init;
        PluginDestroyFunc destroy;
        PluginStartFunc   start;
        PluginStopFunc    stop;
        PluginGetInfoFunc get_info;

        void*    user_data;
        uint64_t load_time;
        uint64_t last_active_time;
    };

    PluginManager* plugin_manager_create(void);

    void plugin_manager_destroy(PluginManager* manager);

    int plugin_manager_load(PluginManager* manager, const char* plugin_path, Plugin** plugin_out);

    int plugin_manager_unload(PluginManager* manager, const char* plugin_name);

    Plugin* plugin_manager_get(PluginManager* manager, const char* plugin_name);

    Plugin* plugin_manager_get_by_type(PluginManager* manager, PluginType type, size_t index);

    size_t plugin_manager_get_count(PluginManager* manager);

    size_t plugin_manager_get_count_by_type(PluginManager* manager, PluginType type);

    int plugin_manager_start_all(PluginManager* manager);

    int plugin_manager_stop_all(PluginManager* manager);

    int plugin_manager_reload(PluginManager* manager, const char* plugin_name);

    PluginState plugin_get_state(Plugin* plugin);

    const char* plugin_type_to_string(PluginType type);

    const char* plugin_state_to_string(PluginState state);

#ifdef __cplusplus
}
#endif

#endif
