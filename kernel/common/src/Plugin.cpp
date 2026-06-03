#include "bs/kernel/common/Plugin.h"

#include <cstdio>
#include <cstring>

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#define DL_HANDLE void*
#define DL_LOAD(path) reinterpret_cast<void*>(LoadLibraryA(path))
#define DL_UNLOAD(handle) FreeLibrary(reinterpret_cast<HMODULE>(handle))
#define DL_SYM(handle, name) GetProcAddress(reinterpret_cast<HMODULE>(handle), name)
#define DL_ERROR() "Windows error"
#else
#include <dlfcn.h>
#define DL_HANDLE void*
#define DL_LOAD(path) dlopen(path, RTLD_LAZY | RTLD_GLOBAL)
#define DL_UNLOAD(handle) dlclose(handle)
#define DL_SYM(handle, name) dlsym(handle, name)
#define DL_ERROR() dlerror()
#endif

struct PluginManager
{
    std::mutex                                           mutex;
    std::unordered_map<std::string, Plugin*>             plugins;
    std::unordered_map<PluginType, std::vector<Plugin*>> plugins_by_type;
};

PluginManager* bs_plugin_manager_create(void)
{
    return new PluginManager();
}

void bs_plugin_manager_destroy(PluginManager* manager)
{
    if (!manager)
        return;

    {
        std::lock_guard<std::mutex> lock(manager->mutex);

        for (auto& pair : manager->plugins)
        {
            Plugin* plugin = pair.second;
            if (plugin->state == PLUGIN_STATE_ACTIVE)
            {
                plugin->stop(plugin);
            }
            if (plugin->state >= PLUGIN_STATE_LOADED)
            {
                plugin->destroy(plugin);
            }
            if (plugin->handle)
            {
                DL_UNLOAD(plugin->handle);
            }
            delete plugin;
        }
        manager->plugins.clear();
        manager->plugins_by_type.clear();
    }

    delete manager;
}

int bs_plugin_manager_load(PluginManager* manager, const char* plugin_path, Plugin** plugin_out)
{
    if (!manager || !plugin_path || !plugin_out)
        return -1;

    std::lock_guard<std::mutex> lock(manager->mutex);

    DL_HANDLE handle = DL_LOAD(plugin_path);
    if (!handle)
    {
        return -2;
    }

    Plugin* plugin = new Plugin();
    memset(plugin, 0, sizeof(Plugin));
    plugin->handle = handle;

    plugin->init     = reinterpret_cast<PluginInitFunc>(DL_SYM(handle, "plugin_init"));
    plugin->destroy  = reinterpret_cast<PluginDestroyFunc>(DL_SYM(handle, "plugin_destroy"));
    plugin->start    = reinterpret_cast<PluginStartFunc>(DL_SYM(handle, "plugin_start"));
    plugin->stop     = reinterpret_cast<PluginStopFunc>(DL_SYM(handle, "plugin_stop"));
    plugin->get_info = reinterpret_cast<PluginGetInfoFunc>(DL_SYM(handle, "plugin_get_info"));

    if (!plugin->init || !plugin->destroy)
    {
        DL_UNLOAD(handle);
        delete plugin;
        return -3;
    }

    int result = plugin->init(plugin);
    if (result != 0)
    {
        DL_UNLOAD(handle);
        delete plugin;
        return -4;
    }

    plugin->state = PLUGIN_STATE_LOADED;

    if (plugin->start)
    {
        result = plugin->start(plugin);
        if (result == 0)
        {
            plugin->state = PLUGIN_STATE_ACTIVE;
        }
    }

    manager->plugins[plugin->name ? plugin->name : "unknown"] = plugin;
    manager->plugins_by_type[plugin->type].push_back(plugin);

    *plugin_out = plugin;
    return 0;
}

int bs_plugin_manager_unload(PluginManager* manager, const char* plugin_name)
{
    if (!manager || !plugin_name)
        return -1;

    std::lock_guard<std::mutex> lock(manager->mutex);

    auto it = manager->plugins.find(plugin_name);
    if (it == manager->plugins.end())
    {
        return -2;
    }

    Plugin* plugin = it->second;

    if (plugin->state == PLUGIN_STATE_ACTIVE && plugin->stop)
    {
        plugin->stop(plugin);
    }

    if (plugin->destroy)
    {
        plugin->destroy(plugin);
    }

    if (plugin->handle)
    {
        DL_UNLOAD(plugin->handle);
    }

    auto& type_list = manager->plugins_by_type[plugin->type];
    for (auto iter = type_list.begin(); iter != type_list.end(); ++iter)
    {
        if (*iter == plugin)
        {
            type_list.erase(iter);
            break;
        }
    }

    manager->plugins.erase(it);
    delete plugin;

    return 0;
}

Plugin* bs_plugin_manager_get(PluginManager* manager, const char* plugin_name)
{
    if (!manager || !plugin_name)
        return nullptr;

    std::lock_guard<std::mutex> lock(manager->mutex);

    auto it = manager->plugins.find(plugin_name);
    return (it != manager->plugins.end()) ? it->second : nullptr;
}

Plugin* bs_plugin_manager_get_by_type(PluginManager* manager, PluginType type, size_t index)
{
    if (!manager)
        return nullptr;

    std::lock_guard<std::mutex> lock(manager->mutex);

    auto it = manager->plugins_by_type.find(type);
    if (it == manager->plugins_by_type.end())
        return nullptr;

    const auto& list = it->second;
    return (index < list.size()) ? list[index] : nullptr;
}

size_t bs_plugin_manager_get_count(PluginManager* manager)
{
    if (!manager)
        return 0;

    std::lock_guard<std::mutex> lock(manager->mutex);
    return manager->plugins.size();
}

size_t bs_plugin_manager_get_count_by_type(PluginManager* manager, PluginType type)
{
    if (!manager)
        return 0;

    std::lock_guard<std::mutex> lock(manager->mutex);

    auto it = manager->plugins_by_type.find(type);
    return (it != manager->plugins_by_type.end()) ? it->second.size() : 0;
}

int bs_plugin_manager_start_all(PluginManager* manager)
{
    if (!manager)
        return -1;

    std::lock_guard<std::mutex> lock(manager->mutex);

    int success_count = 0;
    for (auto& pair : manager->plugins)
    {
        Plugin* plugin = pair.second;
        if (plugin->state == PLUGIN_STATE_LOADED && plugin->start)
        {
            if (plugin->start(plugin) == 0)
            {
                plugin->state = PLUGIN_STATE_ACTIVE;
                success_count++;
            }
        }
    }

    return success_count;
}

int bs_plugin_manager_stop_all(PluginManager* manager)
{
    if (!manager)
        return -1;

    std::lock_guard<std::mutex> lock(manager->mutex);

    int success_count = 0;
    for (auto& pair : manager->plugins)
    {
        Plugin* plugin = pair.second;
        if (plugin->state == PLUGIN_STATE_ACTIVE && plugin->stop)
        {
            if (plugin->stop(plugin) == 0)
            {
                plugin->state = PLUGIN_STATE_LOADED;
                success_count++;
            }
        }
    }

    return success_count;
}

int bs_plugin_manager_reload(PluginManager* manager, const char* plugin_name)
{
    if (!manager || !plugin_name)
        return -1;

    Plugin* plugin = bs_plugin_manager_get(manager, plugin_name);
    if (!plugin)
        return -2;

    const char* path = plugin->get_info ? plugin->get_info(plugin, "path") : nullptr;
    if (!path)
        return -3;

    int result = bs_plugin_manager_unload(manager, plugin_name);
    if (result != 0)
        return -4;

    Plugin* new_plugin = nullptr;
    result             = bs_plugin_manager_load(manager, path, &new_plugin);
    return result;
}

PluginState bs_plugin_get_state(Plugin* plugin)
{
    if (!plugin)
        return PLUGIN_STATE_UNLOADED;
    return plugin->state;
}

const char* bs_plugin_type_to_string(PluginType type)
{
    switch (type)
    {
    case PLUGIN_TYPE_FORMAT_PARSER:
        return "FORMAT_PARSER";
    case PLUGIN_TYPE_SCHEMA_LOADER:
        return "SCHEMA_LOADER";
    case PLUGIN_TYPE_VALIDATOR:
        return "VALIDATOR";
    case PLUGIN_TYPE_IR_GENERATOR:
        return "IR_GENERATOR";
    case PLUGIN_TYPE_EXECUTOR:
        return "EXECUTOR";
    case PLUGIN_TYPE_OBSERVER:
        return "OBSERVER";
    default:
        return "UNKNOWN";
    }
}

const char* bs_plugin_state_to_string(PluginState state)
{
    switch (state)
    {
    case PLUGIN_STATE_UNLOADED:
        return "UNLOADED";
    case PLUGIN_STATE_LOADED:
        return "LOADED";
    case PLUGIN_STATE_ACTIVE:
        return "ACTIVE";
    case PLUGIN_STATE_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}
