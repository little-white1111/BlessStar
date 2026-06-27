/* ── executor_plugin.cpp ──────────────────────────────────────────────
 * Thin Plugin-wrapper for bs_kernel_pipeline executor.
 * Registers as PLUGIN_TYPE_EXECUTOR for hot-reload.
 * DAY38-14: Plugin 热重载全面接入 — Executor 扩展点
 * ──────────────────────────────────────────────────────────────────── */

#include <bs/kernel/common/Plugin.h>
#include <bs/kernel/pipeline/pipeline.h>
#include <bs/kernel/ir/ir.h>
#include <bs/kernel/report/report.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

/* ── Plugin user data ──────────────────────────────────────────────── */
typedef struct ExecutorPluginData {
    Pipeline* pipeline;
} ExecutorPluginData;

/* ── init ──────────────────────────────────────────────────────────── */
static int executor_plugin_init(Plugin* plugin)
{
    ExecutorPluginData* data = (ExecutorPluginData*)calloc(1, sizeof(ExecutorPluginData));
    if (!data) return -1;
    data->pipeline = bs_pipeline_create();
    if (!data->pipeline) {
        free(data);
        return -2;
    }
    plugin->user_data = data;
    plugin->state     = PLUGIN_STATE_LOADED;
    plugin->load_time = 0;
    return 0;
}

/* ── start ─────────────────────────────────────────────────────────── */
static int executor_plugin_start(Plugin* plugin)
{
    if (!plugin || !plugin->user_data) return -1;
    plugin->state = PLUGIN_STATE_ACTIVE;
    return 0;
}

/* ── stop ──────────────────────────────────────────────────────────── */
static int executor_plugin_stop(Plugin* plugin)
{
    if (!plugin) return -1;
    plugin->state = PLUGIN_STATE_LOADED;
    return 0;
}

/* ── destroy ───────────────────────────────────────────────────────── */
static int executor_plugin_destroy(Plugin* plugin)
{
    if (!plugin || !plugin->user_data) return -1;
    ExecutorPluginData* data = (ExecutorPluginData*)plugin->user_data;
    if (data->pipeline) {
        bs_pipeline_destroy(data->pipeline);
        data->pipeline = nullptr;
    }
    free(data);
    plugin->user_data = nullptr;
    plugin->state     = PLUGIN_STATE_UNLOADED;
    return 0;
}

/* ── get_info ──────────────────────────────────────────────────────── */
static const char* executor_plugin_get_info(Plugin* plugin, const char* key)
{
    if (!plugin || !key) return nullptr;
    if (strcmp(key, "version") == 0) return "1.0";
    if (strcmp(key, "type_name") == 0) return "pipeline_executor";
    if (strcmp(key, "capabilities") == 0)
        return "pipeline_execute,pipeline_add_stage,pipeline_reset";
    return nullptr;
}

/* ── Factory function ──────────────────────────────────────────────── */
extern "C" Plugin* bs_executor_plugin_create(void)
{
    Plugin* p = (Plugin*)calloc(1, sizeof(Plugin));
    if (!p) return nullptr;

    p->name    = "pipeline_executor";
    p->version = "1.0.0";
    p->type    = PLUGIN_TYPE_EXECUTOR;
    p->state   = PLUGIN_STATE_UNLOADED;
    p->handle  = nullptr;

    p->init     = executor_plugin_init;
    p->destroy  = executor_plugin_destroy;
    p->start    = executor_plugin_start;
    p->stop     = executor_plugin_stop;
    p->get_info = executor_plugin_get_info;

    p->user_data        = nullptr;
    p->load_time        = 0;
    p->last_active_time = 0;

    return p;
}
