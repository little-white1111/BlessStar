#include "bs/kernel/common/Plugin.h"

#include <cassert>
#include <cstdio>
#include <cstring>

static int test_plugin_init(Plugin* plugin)
{
    plugin->name    = "test_plugin";
    plugin->version = "1.0.0";
    plugin->type    = PLUGIN_TYPE_VALIDATOR;
    return 0;
}

static int test_plugin_destroy(Plugin* plugin)
{
    plugin->state = PLUGIN_STATE_UNLOADED;
    return 0;
}

static int test_plugin_start(Plugin* plugin)
{
    plugin->state = PLUGIN_STATE_ACTIVE;
    return 0;
}

static int test_plugin_stop(Plugin* plugin)
{
    plugin->state = PLUGIN_STATE_LOADED;
    return 0;
}

static const char* test_plugin_get_info(Plugin* plugin, const char* key)
{
    (void)plugin;
    (void)key;
    static const char* info = "test_info";
    return info;
}

static void test_PluginManager_CreateDestroy()
{
    PluginManager* manager = bs_plugin_manager_create();
    assert(manager != nullptr);
    bs_plugin_manager_destroy(manager);
    printf("test_PluginManager_CreateDestroy: PASS\n");
}

static void test_Plugin_CreateDestroy()
{
    Plugin* plugin = new Plugin();
    memset(plugin, 0, sizeof(Plugin));

    plugin->name    = "test";
    plugin->version = "1.0.0";
    plugin->type    = PLUGIN_TYPE_FORMAT_PARSER;
    plugin->state   = PLUGIN_STATE_LOADED;

    plugin->init     = test_plugin_init;
    plugin->destroy  = test_plugin_destroy;
    plugin->start    = test_plugin_start;
    plugin->stop     = test_plugin_stop;
    plugin->get_info = test_plugin_get_info;

    assert(plugin->init(plugin) == 0);
    assert(plugin->start(plugin) == 0);
    assert(plugin->state == PLUGIN_STATE_ACTIVE);

    assert(plugin->stop(plugin) == 0);
    assert(plugin->state == PLUGIN_STATE_LOADED);

    assert(plugin->destroy(plugin) == 0);
    assert(plugin->state == PLUGIN_STATE_UNLOADED);

    delete plugin;
    printf("test_Plugin_CreateDestroy: PASS\n");
}

static void test_Plugin_StateTransitions()
{
    Plugin* plugin = new Plugin();
    memset(plugin, 0, sizeof(Plugin));

    plugin->state = PLUGIN_STATE_UNLOADED;
    assert(bs_plugin_get_state(plugin) == PLUGIN_STATE_UNLOADED);

    plugin->state = PLUGIN_STATE_LOADED;
    assert(bs_plugin_get_state(plugin) == PLUGIN_STATE_LOADED);

    plugin->state = PLUGIN_STATE_ACTIVE;
    assert(bs_plugin_get_state(plugin) == PLUGIN_STATE_ACTIVE);

    plugin->state = PLUGIN_STATE_ERROR;
    assert(bs_plugin_get_state(plugin) == PLUGIN_STATE_ERROR);

    delete plugin;
    printf("test_Plugin_StateTransitions: PASS\n");
}

static void test_Plugin_TypeToString()
{
    assert(strcmp(bs_plugin_type_to_string(PLUGIN_TYPE_FORMAT_PARSER), "FORMAT_PARSER") == 0);
    assert(strcmp(bs_plugin_type_to_string(PLUGIN_TYPE_SCHEMA_LOADER), "SCHEMA_LOADER") == 0);
    assert(strcmp(bs_plugin_type_to_string(PLUGIN_TYPE_VALIDATOR), "VALIDATOR") == 0);
    assert(strcmp(bs_plugin_type_to_string(PLUGIN_TYPE_IR_GENERATOR), "IR_GENERATOR") == 0);
    assert(strcmp(bs_plugin_type_to_string(PLUGIN_TYPE_EXECUTOR), "EXECUTOR") == 0);
    assert(strcmp(bs_plugin_type_to_string(PLUGIN_TYPE_OBSERVER), "OBSERVER") == 0);
    printf("test_Plugin_TypeToString: PASS\n");
}

static void test_Plugin_StateToString()
{
    assert(strcmp(bs_plugin_state_to_string(PLUGIN_STATE_UNLOADED), "UNLOADED") == 0);
    assert(strcmp(bs_plugin_state_to_string(PLUGIN_STATE_LOADED), "LOADED") == 0);
    assert(strcmp(bs_plugin_state_to_string(PLUGIN_STATE_ACTIVE), "ACTIVE") == 0);
    assert(strcmp(bs_plugin_state_to_string(PLUGIN_STATE_ERROR), "ERROR") == 0);
    printf("test_Plugin_StateToString: PASS\n");
}

static void test_PluginManager_GetCount()
{
    PluginManager* manager = bs_plugin_manager_create();
    assert(bs_plugin_manager_get_count(manager) == 0);
    bs_plugin_manager_destroy(manager);
    printf("test_PluginManager_GetCount: PASS\n");
}

static void test_PluginManager_GetByType()
{
    PluginManager* manager = bs_plugin_manager_create();
    Plugin*        plugin  = bs_plugin_manager_get_by_type(manager, PLUGIN_TYPE_VALIDATOR, 0);
    assert(plugin == nullptr);
    bs_plugin_manager_destroy(manager);
    printf("test_PluginManager_GetByType: PASS\n");
}

static void test_PluginManager_NullInput()
{
    PluginManager* manager = bs_plugin_manager_create();

    int result = bs_plugin_manager_load(nullptr, "test.so", nullptr);
    assert(result == -1);

    result = bs_plugin_manager_unload(nullptr, "test");
    assert(result == -1);

    result = bs_plugin_manager_unload(manager, nullptr);
    assert(result == -1);

    Plugin* plugin = bs_plugin_manager_get(nullptr, "test");
    assert(plugin == nullptr);

    bs_plugin_manager_destroy(nullptr);
    bs_plugin_manager_destroy(manager);
    printf("test_PluginManager_NullInput: PASS\n");
}

int main()
{
    printf("=== Plugin Tests ===\n");
    test_PluginManager_CreateDestroy();
    test_Plugin_CreateDestroy();
    test_Plugin_StateTransitions();
    test_Plugin_TypeToString();
    test_Plugin_StateToString();
    test_PluginManager_GetCount();
    test_PluginManager_GetByType();
    test_PluginManager_NullInput();
    printf("=== All Plugin Tests PASSED! ===\n");
    return 0;
}
