/* ── schema_validator_plugin.c ────────────────────────────────────────
 * Thin Plugin-wrapper for bs_kernel_schema validator.
 * Registers schema-validate as PLUGIN_TYPE_VALIDATOR.
 * DAY38-14: Plugin 热重载全面接入 — Validator 扩展点
 * ──────────────────────────────────────────────────────────────────── */

#include <bs/kernel/common/Plugin.h>
#include <bs/kernel/schema/schema_validator.h>
#include <bs/kernel/schema/schema_types.h>

#include <stdlib.h>
#include <string.h>

/* ── Plugin user data ──────────────────────────────────────────────── */
typedef struct SchemaValidatorPluginData {
    /* Custom validator lookup context */
    bs_validator_lookup_fn lookup_fn;
    void*                  lookup_ctx;
} SchemaValidatorPluginData;

/* ── init ──────────────────────────────────────────────────────────── */
static int schema_validator_plugin_init(Plugin* plugin)
{
    SchemaValidatorPluginData* data =
        (SchemaValidatorPluginData*)calloc(1, sizeof(SchemaValidatorPluginData));
    if (!data) return -1;
    data->lookup_fn  = NULL;
    data->lookup_ctx = NULL;
    plugin->user_data = data;
    plugin->state     = PLUGIN_STATE_LOADED;
    plugin->load_time = 0;
    return 0;
}

/* ── start ─────────────────────────────────────────────────────────── */
static int schema_validator_plugin_start(Plugin* plugin)
{
    if (!plugin) return -1;
    plugin->state = PLUGIN_STATE_ACTIVE;
    return 0;
}

/* ── stop ──────────────────────────────────────────────────────────── */
static int schema_validator_plugin_stop(Plugin* plugin)
{
    if (!plugin) return -1;
    plugin->state = PLUGIN_STATE_LOADED;
    return 0;
}

/* ── destroy ───────────────────────────────────────────────────────── */
static int schema_validator_plugin_destroy(Plugin* plugin)
{
    if (!plugin || !plugin->user_data) return -1;
    free(plugin->user_data);
    plugin->user_data = NULL;
    plugin->state     = PLUGIN_STATE_UNLOADED;
    return 0;
}

/* ── get_info ──────────────────────────────────────────────────────── */
static const char* schema_validator_plugin_get_info(Plugin* plugin, const char* key)
{
    if (!plugin || !key) return NULL;
    if (strcmp(key, "version") == 0) return "1.0";
    if (strcmp(key, "type_name") == 0) return "schema_validator";
    if (strcmp(key, "capabilities") == 0)
        return "type_check,range_check,pattern_match,enum_check,custom_validator";
    return NULL;
}

/* ── Factory function ──────────────────────────────────────────────── */
Plugin* bs_schema_validator_plugin_create(void)
{
    Plugin* p = (Plugin*)calloc(1, sizeof(Plugin));
    if (!p) return NULL;

    p->name    = "schema_validator";
    p->version = "1.0.0";
    p->type    = PLUGIN_TYPE_VALIDATOR;
    p->state   = PLUGIN_STATE_UNLOADED;
    p->handle  = NULL;

    p->init     = schema_validator_plugin_init;
    p->destroy  = schema_validator_plugin_destroy;
    p->start    = schema_validator_plugin_start;
    p->stop     = schema_validator_plugin_stop;
    p->get_info = schema_validator_plugin_get_info;

    p->user_data        = NULL;
    p->load_time        = 0;
    p->last_active_time = 0;

    return p;
}
