#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bs/kernel/common/bs_log.h>
#include <bs/kernel/schema/gold_standard.h>

#include "schema_loader_internal.h"

/* ── Domain ID for logging (schema_loader) ─────────────────────────── */
#define BS_LOG_DOMAIN_SCHEMA_LOADER 0x1101

/* ── Create (标准 config-schema.yaml) ──────────────────────────────── */
struct bs_schema_loader* bs_schema_loader_create(const char* yaml_path)
{
    struct bs_schema_loader* loader =
        (struct bs_schema_loader*)calloc(1, sizeof(struct bs_schema_loader));
    if (!loader)
        return NULL;

    if (yaml_path)
    {
        loader->yaml_path = strdup(yaml_path);
        if (!loader->yaml_path)
        {
            free(loader);
            return NULL;
        }
    }

    /* Try initial load on creation */
    if (loader->yaml_path)
    {
        int rc = bs_schema_loader_reload(loader);
        if (rc != 0)
        {
            /* Load failed; active_schema stays NULL, Warn already logged */
        }
    }
    else
    {
        bs_log_emit(BS_LOG_DOMAIN_SCHEMA_LOADER, BS_LOG_WARN,
                    "SchemaLoader: no yaml_path provided, starting with empty ruleset");
    }

    return loader;
}

/* ── Create (bundled config-schema.bundled.yaml 缓存格式) ──────────── */
/* 与标准创建函数相同，但使用 bs_schema_yaml_parse_bundled 解析。
 * 适用于编译期聚合后的精简缓存格式。 */
struct bs_schema_loader* bs_schema_loader_create_bundled(const char* yaml_path)
{
    struct bs_schema_loader* loader =
        (struct bs_schema_loader*)calloc(1, sizeof(struct bs_schema_loader));
    if (!loader)
        return NULL;

    if (yaml_path)
    {
        loader->yaml_path = strdup(yaml_path);
        if (!loader->yaml_path)
        {
            free(loader);
            return NULL;
        }
    }

    /* Try initial load using bundled parser */
    if (loader->yaml_path)
    {
        int rc = bs_schema_loader_reload_bundled(loader);
        if (rc != 0)
        {
            /* Load failed; active_schema stays NULL, Warn already logged */
        }
    }
    else
    {
        bs_log_emit(BS_LOG_DOMAIN_SCHEMA_LOADER, BS_LOG_WARN,
                    "SchemaLoader(bundled): no yaml_path provided, starting with empty ruleset");
    }

    return loader;
}

/* ── Destroy ───────────────────────────────────────────────────────── */
void bs_schema_loader_destroy(struct bs_schema_loader* loader)
{
    if (!loader)
        return;
    free(loader->yaml_path);
    bs_schema_free(loader->active_schema);
    free(loader);
}

/* ── Check required fields ─────────────────────────────────────────── */
static int check_required_fields(const struct bs_schema* schema)
{
    if (!schema)
        return -1;

    for (size_t i = 0; i < BS_GOLD_STANDARD_V1_COUNT; i++)
    {
        if (!BS_GOLD_STANDARD_V1[i].required)
            continue;

        int found = 0;
        for (size_t j = 0; j < schema->field_count; j++)
        {
            if (strcmp(schema->fields[j].qualified_name, BS_GOLD_STANDARD_V1[i].qualified_name) ==
                0)
            {
                found = 1;
                break;
            }
        }
        if (!found)
        {
            bs_log_emit(BS_LOG_DOMAIN_SCHEMA_LOADER, BS_LOG_ERROR,
                        "SchemaLoader: required field '%s' missing in YAML",
                        BS_GOLD_STANDARD_V1[i].qualified_name);
            return -1;
        }
    }
    return 0;
}

/* ── Apply default values for missing optional fields ──────────────── */
static int apply_defaults(struct bs_schema* schema)
{
    if (!schema)
        return -1;

    for (size_t i = 0; i < BS_GOLD_STANDARD_V1_COUNT; i++)
    {
        if (BS_GOLD_STANDARD_V1[i].required)
            continue;

        int found = 0;
        for (size_t j = 0; j < schema->field_count; j++)
        {
            if (strcmp(schema->fields[j].qualified_name, BS_GOLD_STANDARD_V1[i].qualified_name) ==
                0)
            {
                found = 1;
                break;
            }
        }
        if (!found)
        {
            /* Add default value if one exists */
            if (BS_GOLD_STANDARD_V1[i].default_value)
            {
                struct bs_schema_field* new_fields = (struct bs_schema_field*)realloc(
                    schema->fields, (schema->field_count + 1) * sizeof(struct bs_schema_field));
                if (!new_fields)
                    return -1;
                schema->fields = new_fields;

                struct bs_schema_field* f = &schema->fields[schema->field_count];
                f->qualified_name         = strdup(BS_GOLD_STANDARD_V1[i].qualified_name);
                f->value                  = strdup(BS_GOLD_STANDARD_V1[i].default_value);
                f->type                   = BS_GOLD_STANDARD_V1[i].type;
                f->required               = 0;
                schema->field_count++;
            }
        }
    }
    return 0;
}

/* ── Reload (atomic switch: three-phase) ───────────────────────────── */
int bs_schema_loader_reload(struct bs_schema_loader* loader)
{
    if (!loader)
        return -1;

    bs_log_emit(BS_LOG_DOMAIN_SCHEMA_LOADER, BS_LOG_INFO, "SchemaLoader: reload triggered for '%s'",
                loader->yaml_path ? loader->yaml_path : "(null)");

    /* Phase 1: Parse YAML into temp schema */
    struct bs_schema* temp = NULL;
    int               rc   = bs_schema_yaml_parse(loader->yaml_path, &temp);
    if (rc != 0)
    {
        bs_log_emit(BS_LOG_DOMAIN_SCHEMA_LOADER, BS_LOG_ERROR,
                    "SchemaLoader: YAML parse failed (rc=%d), keeping old schema", rc);
        return rc;
    }

    /* Phase 1a: YAML file not found → empty ruleset */
    if (!temp)
    {
        bs_log_emit(BS_LOG_DOMAIN_SCHEMA_LOADER, BS_LOG_WARN,
                    "SchemaLoader: YAML not found at '%s', using empty ruleset",
                    loader->yaml_path ? loader->yaml_path : "(null)");
        struct bs_schema* old = loader->active_schema;
        loader->active_schema = NULL;

        if (old)
        {
            bs_schema_free(old);
            if (loader->on_switch)
            {
                loader->on_switch(NULL, loader->switch_userdata);
            }
        }
        return 0;
    }

    /* Phase 2: Validate required fields against gold standard */
    if (check_required_fields(temp) != 0)
    {
        bs_log_emit(BS_LOG_DOMAIN_SCHEMA_LOADER, BS_LOG_ERROR,
                    "SchemaLoader: validation failed, keeping old schema");
        bs_schema_free(temp);
        return -1;
    }

    /* Phase 2b: Apply default values for optional fields */
    if (apply_defaults(temp) != 0)
    {
        bs_log_emit(BS_LOG_DOMAIN_SCHEMA_LOADER, BS_LOG_ERROR,
                    "SchemaLoader: apply_defaults failed, keeping old schema");
        bs_schema_free(temp);
        return -1;
    }

    /* Phase 3: Atomic pointer swap */
    struct bs_schema* old = loader->active_schema;
    loader->active_schema = temp;

    bs_schema_free(old);

    bs_log_emit(BS_LOG_DOMAIN_SCHEMA_LOADER, BS_LOG_INFO,
                "SchemaLoader: reload successful (%zu fields)", temp->field_count);

    /* Fire callback */
    if (loader->on_switch)
    {
        loader->on_switch(temp, loader->switch_userdata);
    }

    return 0;
}

/* ── Get active schema (read-only) ─────────────────────────────────── */
const struct bs_schema* bs_schema_loader_get_active(const struct bs_schema_loader* loader)
{
    if (!loader)
        return NULL;
    return loader->active_schema;
}

/* ── Reload (bundled 版本：使用 bundled YAML 解析器) ────────────────── */
int bs_schema_loader_reload_bundled(struct bs_schema_loader* loader)
{
    if (!loader)
        return -1;

    bs_log_emit(BS_LOG_DOMAIN_SCHEMA_LOADER, BS_LOG_INFO,
                "SchemaLoader(bundled): reload triggered for '%s'",
                loader->yaml_path ? loader->yaml_path : "(null)");

    /* Phase 1: Parse bundled YAML into temp schema */
    struct bs_schema* temp = NULL;
    int               rc   = bs_schema_yaml_parse_bundled(loader->yaml_path, &temp);
    if (rc != 0)
    {
        bs_log_emit(BS_LOG_DOMAIN_SCHEMA_LOADER, BS_LOG_ERROR,
                    "SchemaLoader(bundled): YAML parse failed (rc=%d), keeping old schema", rc);
        return rc;
    }

    /* Phase 1a: YAML file not found → empty ruleset */
    if (!temp)
    {
        bs_log_emit(BS_LOG_DOMAIN_SCHEMA_LOADER, BS_LOG_WARN,
                    "SchemaLoader(bundled): YAML not found at '%s', using empty ruleset",
                    loader->yaml_path ? loader->yaml_path : "(null)");
        struct bs_schema* old = loader->active_schema;
        loader->active_schema = NULL;

        if (old)
        {
            bs_schema_free(old);
            if (loader->on_switch)
            {
                loader->on_switch(NULL, loader->switch_userdata);
            }
        }
        return 0;
    }

    /* Phase 2: bundled.yaml 是自包含缓存格式，跳过 gold standard 校验和默认值注入 */
    /* (gold standard 仅用于 config-schema.yaml 原始格式) */

    /* Phase 3: Atomic pointer swap */
    struct bs_schema* old = loader->active_schema;
    loader->active_schema = temp;

    bs_schema_free(old);

    bs_log_emit(BS_LOG_DOMAIN_SCHEMA_LOADER, BS_LOG_INFO,
                "SchemaLoader(bundled): reload successful (%zu fields)", temp->field_count);

    /* Fire callback */
    if (loader->on_switch)
    {
        loader->on_switch(temp, loader->switch_userdata);
    }

    return 0;
}

/* ── Register switch callback ──────────────────────────────────────── */
int bs_schema_loader_on_switch(struct bs_schema_loader* loader, bs_schema_switch_callback callback,
                               void* userdata)
{
    if (!loader)
        return -1;
    loader->on_switch       = callback;
    loader->switch_userdata = userdata;
    return 0;
}
