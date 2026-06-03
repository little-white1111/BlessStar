#include "bs/kernel/state/ConfigManager.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static int g_active_watch_hits = 0;

static void on_state_watch(const char* path, ConfigEventType type, const void* data, void* userData)
{
    (void)userData;
    (void)data;
    if (path && std::strcmp(path, "/finance/cfg-a") == 0 && type == CONFIG_EVENT_ENTER_ACTIVE)
        g_active_watch_hits++;
}

static void test_load_get_snapshot()
{
    ConfigManager* cm = bs_config_manager_create();
    assert(cm != nullptr);

    const char payload[] = "v1-bytes";
    assert(bs_config_manager_load_config(cm, "/finance/cfg-a", payload, sizeof(payload)) == 0);

    ConfigState state = CONFIG_STATE_INITIAL;
    assert(bs_config_manager_get_config_state(cm, "/finance/cfg-a", &state) == 0);
    assert(state == CONFIG_STATE_ACTIVE);

    void*  snap = nullptr;
    size_t sz   = 0;
    assert(bs_config_manager_get_config_snapshot(cm, "/finance/cfg-a", &snap, &sz) == 0);
    assert(sz == sizeof(payload));
    assert(std::memcmp(snap, payload, sz) == 0);
    std::free(snap);

    assert(bs_config_manager_load_config(cm, "/finance/cfg-a", payload, sizeof(payload)) == -3);

    bs_config_manager_destroy(cm);
    std::printf("test_load_get_snapshot: PASS\n");
}

static void test_reload_hot_update_watch()
{
    ConfigManager* cm = bs_config_manager_create();
    assert(cm != nullptr);

    g_active_watch_hits = 0;
    assert(bs_config_manager_subscribe_state_change(cm, "/finance/cfg-a", on_state_watch,
                                                    nullptr) == 0);

    const char v1[] = "version-1";
    assert(bs_config_manager_load_config(cm, "/finance/cfg-a", v1, sizeof(v1)) == 0);
    assert(g_active_watch_hits == 1);

    const char v2[] = "version-2";
    assert(bs_config_manager_reload_config(cm, "/finance/cfg-a", v2, sizeof(v2)) == 0);
    assert(g_active_watch_hits == 2);

    const char v3[] = "version-3";
    assert(bs_config_manager_hot_update(cm, "/finance/cfg-a", v3, sizeof(v3)) == 0);
    assert(g_active_watch_hits == 3);

    void*  snap = nullptr;
    size_t sz   = 0;
    assert(bs_config_manager_get_config_snapshot(cm, "/finance/cfg-a", &snap, &sz) == 0);
    assert(sz == sizeof(v3));
    assert(std::memcmp(snap, v3, sz) == 0);
    std::free(snap);

    bs_config_manager_destroy(cm);
    std::printf("test_reload_hot_update_watch: PASS\n");
}

static void test_unload()
{
    ConfigManager* cm = bs_config_manager_create();
    assert(cm != nullptr);

    const char payload[] = "x";
    assert(bs_config_manager_load_config(cm, "/finance/cfg-b", payload, sizeof(payload)) == 0);
    assert(bs_config_manager_unload_config(cm, "/finance/cfg-b") == 0);

    ConfigState state = CONFIG_STATE_INITIAL;
    assert(bs_config_manager_get_config_state(cm, "/finance/cfg-b", &state) == 0);
    assert(state == CONFIG_STATE_CLOSED);

    assert(bs_config_manager_unload_config(cm, "/missing") == -2);

    bs_config_manager_destroy(cm);
    std::printf("test_unload: PASS\n");
}

int main()
{
    std::printf("=== ConfigManager Tests ===\n");
    test_load_get_snapshot();
    test_reload_hot_update_watch();
    test_unload();
    std::printf("=== All ConfigManager Tests PASSED ===\n");
    return 0;
}
