/* ── gate_chain_plugin.cpp ────────────────────────────────────────────
 * Thin Plugin-wrapper for bs_kernel_gate_chain.
 * Registers gate-factory / gate-chain / gate-evaluator lookup
 * as PLUGIN_TYPE_EXECUTOR so PluginManager can hot-reload.
 * DAY38-14: Plugin 热重载全面接入 — Gate 扩展点
 * ──────────────────────────────────────────────────────────────────── */

#include <bs/kernel/common/Plugin.h>
#include <bs/kernel/gate_chain/gate_factory.h>
#include <bs/kernel/gate_chain/gate_chain_types.h>
#include <bs/kernel/gate_chain/gate_evaluator.h>
#include <bs/kernel/gate_chain/gate_chain_serialize.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

/* ── Plugin user data: holds ref-counted gate_chain ───────────────── */
typedef struct GateChainPluginData {
    bs_gate_chain_t* chain;
} GateChainPluginData;

/* ── init: allocate gate chain ────────────────────────────────────── */
static int gate_chain_plugin_init(Plugin* plugin)
{
    GateChainPluginData* data = (GateChainPluginData*)calloc(1, sizeof(GateChainPluginData));
    if (!data) return -1;
    data->chain = bs_gate_chain_create();
    if (!data->chain) {
        free(data);
        return -2;
    }
    plugin->user_data = data;
    plugin->state = PLUGIN_STATE_LOADED;
    plugin->load_time = 0; /* populated by PluginManager */
    return 0;
}

/* ── start: mark active ───────────────────────────────────────────── */
static int gate_chain_plugin_start(Plugin* plugin)
{
    if (!plugin || !plugin->user_data) return -1;
    plugin->state = PLUGIN_STATE_ACTIVE;
    return 0;
}

/* ── stop: mark loaded (keep chain) ───────────────────────────────── */
static int gate_chain_plugin_stop(Plugin* plugin)
{
    if (!plugin) return -1;
    plugin->state = PLUGIN_STATE_LOADED;
    return 0;
}

/* ── destroy: free gate chain ─────────────────────────────────────── */
static int gate_chain_plugin_destroy(Plugin* plugin)
{
    if (!plugin || !plugin->user_data) return -1;
    GateChainPluginData* data = (GateChainPluginData*)plugin->user_data;
    if (data->chain) {
        bs_gate_chain_free(data->chain);
        data->chain = nullptr;
    }
    free(data);
    plugin->user_data = nullptr;
    plugin->state = PLUGIN_STATE_UNLOADED;
    return 0;
}

/* ── get_info: expose factory/chain/version ───────────────────────── */
static const char* gate_chain_plugin_get_info(Plugin* plugin, const char* key)
{
    if (!plugin || !key) return nullptr;
    if (strcmp(key, "version") == 0) return "1.0";
    if (strcmp(key, "type_name") == 0) return "gate_chain_core";
    if (strcmp(key, "capabilities") == 0)
        return "gate_factory,gate_evaluator,gate_serialize,gate_ast_compile";
    GateChainPluginData* data = (GateChainPluginData*)plugin->user_data;
    if (!data || !data->chain) return nullptr;
    if (strcmp(key, "chain_version") == 0) return data->chain->version;
    return nullptr;
}

/* ── Factory function: creates a gate_chain Plugin ─────────────────── */
extern "C" Plugin* bs_gate_chain_plugin_create(void)
{
    Plugin* p = (Plugin*)calloc(1, sizeof(Plugin));
    if (!p) return nullptr;

    p->name    = "gate_chain_core";
    p->version = "1.0.0";
    p->type    = PLUGIN_TYPE_EXECUTOR;
    p->state   = PLUGIN_STATE_UNLOADED;
    p->handle  = nullptr;

    p->init     = gate_chain_plugin_init;
    p->destroy  = gate_chain_plugin_destroy;
    p->start    = gate_chain_plugin_start;
    p->stop     = gate_chain_plugin_stop;
    p->get_info = gate_chain_plugin_get_info;

    p->user_data       = nullptr;
    p->load_time       = 0;
    p->last_active_time = 0;

    return p;
}
