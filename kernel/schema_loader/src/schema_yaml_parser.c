/* YAML schema parser (MVP: line-based manual parser).
 * TODO: 接入 libyaml 做完整的 YAML 解析 */

#include <ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bs/kernel/schema/gold_standard.h>

#include "schema_loader_internal.h"

/* ── Forward declarations ──────────────────────────────────────────── */
static int              is_gold_field(const char* qualified_name);
static int              is_gold_field_required(const char* qualified_name);
static bs_schema_type_t gold_field_type(const char* qualified_name);

/* ── Trim whitespace (in-place, modifies start) ────────────────────── */
static const char* trim(const char* s)
{
    if (!s)
        return s;
    /* 跳过前导空白 */
    while (*s && (unsigned char)*s <= ' ')
        s++;
    /* 返回 const char*，不能在原地改尾部 */
    return s;
}

/* ── 去除尾部空白（包括 \r\n），使用可写缓冲区 ───────────────────── */
static void trim_trailing(char* s)
{
    if (!s || !*s)
        return;
    size_t len = strlen(s);
    while (len > 0 && ((unsigned char)s[len - 1] <= ' ' || s[len - 1] == '\r'))
    {
        s[len - 1] = '\0';
        len--;
    }
}

/* ── Check if line is a comment (starts with // or #) ──────────────── */
static int is_comment_or_blank(const char* line)
{
    if (!line || *line == '\0')
        return 1;
    const char* t = trim(line);
    if (*t == '\0' || *t == '#' || *t == '/')
        return 1;
    /* Check for // */
    if (t[0] == '/' && t[1] == '/')
        return 1;
    return 0;
}

/* ── Parse a simple "key: value" line ──────────────────────────────── */
/* Returns 1 if parsed successfully, 0 otherwise.
 * On success, sets *key and *value (caller must free). */
static int parse_kv_line(const char* line, char** key, char** value)
{
    if (!line)
        return 0;
    const char* t = trim(line);
    if (!*t)
        return 0;

    const char* colon = strchr(t, ':');
    if (!colon)
        return 0;

    /* Extract key */
    size_t key_len = (size_t)(colon - t);
    while (key_len > 0 && (unsigned char)t[key_len - 1] <= ' ')
        key_len--;
    if (key_len == 0)
        return 0;

    *key = (char*)malloc(key_len + 1);
    if (!*key)
        return 0;
    strncpy(*key, t, key_len);
    (*key)[key_len] = '\0';

    /* Extract value */
    const char* vstart = colon + 1;
    vstart             = trim(vstart);
    *value             = strdup(vstart);
    if (!*value)
    {
        free(*key);
        return 0;
    }

    return 1;
}

/* ── Parse YAML schema file ────────────────────────────────────────── */
int bs_schema_yaml_parse(const char* yaml_path, struct bs_schema** out)
{
    if (!yaml_path || !out)
        return -1;
    *out = NULL;

    FILE* fp = fopen(yaml_path, "r");
    if (!fp)
    {
        /* YAML file not found is not an error; caller treats as empty schema */
        return 0;
    }

    /* Count lines first for allocation */
    char   buf[4096];
    size_t max_fields = 0;
    while (fgets(buf, sizeof(buf), fp))
    {
        if (is_comment_or_blank(buf))
            continue;
        char *k = NULL, *v = NULL;
        if (parse_kv_line(buf, &k, &v))
        {
            if (is_gold_field(k))
                max_fields++;
            free(k);
            free(v);
        }
    }

    /* Allocate schema */
    struct bs_schema* schema = (struct bs_schema*)calloc(1, sizeof(struct bs_schema));
    if (!schema)
    {
        fclose(fp);
        return -1;
    }

    schema->fields =
        (struct bs_schema_field*)calloc(max_fields + 1, sizeof(struct bs_schema_field));
    if (!schema->fields)
    {
        free(schema);
        fclose(fp);
        return -1;
    }

    /* Rewind and parse */
    rewind(fp);
    size_t idx = 0;
    while (fgets(buf, sizeof(buf), fp) && idx < max_fields)
    {
        if (is_comment_or_blank(buf))
            continue;
        char *k = NULL, *v = NULL;
        if (parse_kv_line(buf, &k, &v))
        {
            if (is_gold_field(k))
            {
                schema->fields[idx].qualified_name = k; /* take ownership */
                schema->fields[idx].value          = v; /* take ownership */
                schema->fields[idx].type           = gold_field_type(k);
                schema->fields[idx].required       = is_gold_field_required(k);
                idx++;
            }
            else
            {
                free(k);
                free(v);
            }
        }
    }
    fclose(fp);

    schema->field_count = idx;
    schema->version     = strdup("1.0");
    if (!schema->version)
    {
        bs_schema_free(schema);
        return -1;
    }

    *out = schema;
    return 0;
}

void bs_schema_free(struct bs_schema* schema)
{
    if (!schema)
        return;
    free(schema->version);
    for (size_t i = 0; i < schema->field_count; i++)
    {
        free(schema->fields[i].qualified_name);
        free(schema->fields[i].value);
    }
    free(schema->fields);
    free(schema);
}

/* ── Type string → bs_schema_type_t 映射 ──────────────────────────── */
static bs_schema_type_t parse_type_string(const char* s)
{
    if (!s)
        return BS_SCHEMA_TYPE_STR;
    if (strcmp(s, "STR") == 0)
        return BS_SCHEMA_TYPE_STR;
    if (strcmp(s, "I32") == 0)
        return BS_SCHEMA_TYPE_I32;
    if (strcmp(s, "I64") == 0)
        return BS_SCHEMA_TYPE_I64;
    if (strcmp(s, "F64") == 0)
        return BS_SCHEMA_TYPE_F64;
    if (strcmp(s, "BOOL") == 0)
        return BS_SCHEMA_TYPE_BOOL;
    if (strcmp(s, "ARR") == 0)
        return BS_SCHEMA_TYPE_ARR;
    if (strcmp(s, "OBJ") == 0)
        return BS_SCHEMA_TYPE_OBJ;
    if (strcmp(s, "ENUM") == 0)
        return BS_SCHEMA_TYPE_ENUM;
    return BS_SCHEMA_TYPE_STR;
}

/* ── 去除外层单引号/双引号（写入 dst，至多 dst_size 字节） ────────── */
static void strip_outer_quotes(const char* s, char* dst, size_t dst_size)
{
    if (!s || dst_size == 0)
        return;
    dst[0]     = '\0';
    size_t len = strlen(s);
    if (len >= 2 && ((s[0] == '\'' && s[len - 1] == '\'') || (s[0] == '"' && s[len - 1] == '"')))
    {
        size_t inner = len - 2;
        if (inner >= dst_size)
            inner = dst_size - 1;
        memcpy(dst, s + 1, inner);
        dst[inner] = '\0';
    }
    else
    {
        size_t copy_len = (len >= dst_size) ? (dst_size - 1) : len;
        memcpy(dst, s, copy_len);
        dst[copy_len] = '\0';
    }
}

/* ── Bundled YAML 解析（config-schema.bundled.yaml 列表格式） ──────── */
/* bundled.yaml 使用 YAML 列表语法:
 *   fields:
 *     - key: <field_name>
 *       type: <STR|I32|F64|BOOL|ARR|OBJ>
 *       default: <value>
 *       contract:
 *         approval_required: <true|false>
 *
 * 此解析器使用状态机逐行解析，跳过 generated_at/generated_from 等元数据。
 * 与标准解析器的区别：
 *   - 不使用 gold standard 过滤（bundled 是自包含缓存格式）
 *   - 支持 YAML 列表项语法（- key: ...）
 *   - 提取 version 作为 schema 版本号 */
int bs_schema_yaml_parse_bundled(const char* yaml_path, struct bs_schema** out)
{
    if (!yaml_path || !out)
        return -1;
    *out = NULL;

    FILE* fp = fopen(yaml_path, "r");
    if (!fp)
    {
        return 0; /* 文件不存在视为空 schema */
    }

    /* Phase 1: 统计字段数（用于预分配） */
    char   buf[8192];
    size_t max_fields        = 0;
    int    in_fields_section = 0;

    while (fgets(buf, sizeof(buf), fp))
    {
        trim_trailing(buf);
        if (is_comment_or_blank(buf))
            continue;
        const char* t = trim(buf);

        if (strcmp(t, "fields:") == 0)
        {
            in_fields_section = 1;
            continue;
        }
        if (!in_fields_section)
            continue;
        if (strncmp(t, "- key:", 6) == 0)
        {
            max_fields++;
        }
    }

    /* Allocate schema */
    struct bs_schema* schema = (struct bs_schema*)calloc(1, sizeof(struct bs_schema));
    if (!schema)
    {
        fclose(fp);
        return -1;
    }

    if (max_fields > 0)
    {
        schema->fields =
            (struct bs_schema_field*)calloc(max_fields, sizeof(struct bs_schema_field));
        if (!schema->fields)
        {
            free(schema);
            fclose(fp);
            return -1;
        }
    }

    /* Phase 2: 用状态机解析字段 */
    rewind(fp);

    enum
    {
        ST_HEADER,
        ST_IN_FIELDS,
        ST_FIELD_ITEM
    } state            = ST_HEADER;
    size_t idx         = 0;
    int    in_contract = 0;

    /* 暂存当前正在解析的字段 */
    char pending_key[4096]     = {0};
    char pending_type[64]      = {0};
    char pending_default[8192] = {0};
    int  pending_required      = 0;

    while (fgets(buf, sizeof(buf), fp))
    {
        trim_trailing(buf);
        if (is_comment_or_blank(buf))
            continue;
        const char* t = trim(buf);

        switch (state)
        {

        /* ── 头部状态：提取 version，跳过 domain/generated_* ────────── */
        case ST_HEADER:
            if (strncmp(t, "version:", 8) == 0)
            {
                const char* v = trim(t + 8);
                /* strip optional 'v' prefix */
                if (*v == 'v' || *v == 'V')
                    v++;
                schema->version = strdup(v);
                if (!schema->version)
                {
                    bs_schema_free(schema);
                    fclose(fp);
                    return -1;
                }
            }
            if (strcmp(t, "fields:") == 0)
            {
                state = ST_IN_FIELDS;
            }
            /* 跳过 domain:, generated_at:, generated_from: 及其列表项 */
            break;

        /* ── fields: 头部（在遇到第一个 - key: 前） ─────────────────── */
        case ST_IN_FIELDS:
            if (strncmp(t, "- key:", 6) == 0)
            {
                /* 保存上一个字段 */
                if (pending_key[0] != '\0' && idx < max_fields)
                {
                    schema->fields[idx].qualified_name = strdup(pending_key);
                    schema->fields[idx].value          = strdup(pending_default);
                    schema->fields[idx].type           = parse_type_string(pending_type);
                    schema->fields[idx].required       = pending_required;
                    idx++;
                }

                /* 重置暂存区 */
                memset(pending_key, 0, sizeof(pending_key));
                memset(pending_type, 0, sizeof(pending_type));
                memset(pending_default, 0, sizeof(pending_default));
                pending_required = 0;
                in_contract      = 0;

                /* 提取字段名 */
                const char* key_val = trim(t + 6);
                strip_outer_quotes(key_val, pending_key, sizeof(pending_key));

                state = ST_FIELD_ITEM;
            }
            break;

        /* ── 字段条目内：解析 type / default / contract ────────────── */
        case ST_FIELD_ITEM:
            /* 新的字段项开始 → 保存当前字段 */
            if (strncmp(t, "- key:", 6) == 0)
            {
                if (pending_key[0] != '\0' && idx < max_fields)
                {
                    schema->fields[idx].qualified_name = strdup(pending_key);
                    schema->fields[idx].value          = strdup(pending_default);
                    schema->fields[idx].type           = parse_type_string(pending_type);
                    schema->fields[idx].required       = pending_required;
                    idx++;
                }

                /* 重置 */
                memset(pending_key, 0, sizeof(pending_key));
                memset(pending_type, 0, sizeof(pending_type));
                memset(pending_default, 0, sizeof(pending_default));
                pending_required = 0;
                in_contract      = 0;

                const char* key_val = trim(t + 6);
                strip_outer_quotes(key_val, pending_key, sizeof(pending_key));

                break;
            }

            /* 字段子键 */
            if (strncmp(t, "type:", 5) == 0)
            {
                const char* type_val = trim(t + 5);
                strip_outer_quotes(type_val, pending_type, sizeof(pending_type));
            }
            else if (strncmp(t, "default:", 8) == 0)
            {
                const char* val = trim(t + 8);
                strip_outer_quotes(val, pending_default, sizeof(pending_default));
            }
            else if (strcmp(t, "contract:") == 0)
            {
                in_contract = 1;
            }
            else if (in_contract && strncmp(t, "approval_required:", 18) == 0)
            {
                const char* req_val = trim(t + 18);
                pending_required    = (strcmp(req_val, "true") == 0);
            }
            else if (in_contract && strncmp(t, "immutable:", 10) == 0)
            {
                /* 当前 schema 不存储 immutable，仅解析以维持状态机 */
            }
            else if (in_contract && strncmp(t, "range:", 6) == 0)
            {
                /* 跳过 range */
            }
            else if (in_contract && strncmp(t, "slo_impact:", 11) == 0)
            {
                /* 跳过 slo_impact */
            }
            else if (in_contract && strncmp(t, "dependencies:", 13) == 0)
            {
                /* 跳过 dependencies */
            }
            else if (in_contract && strncmp(t, "- ", 2) == 0)
            {
                /* 跳过 dependencies 列表项 */
            }
            else
            {
                /* 非 contract 子键 → 退出 contract 状态 */
                in_contract = 0;
            }
            break;
        }
    }

    /* 保存最后一个字段 */
    if (pending_key[0] != '\0' && idx < max_fields)
    {
        schema->fields[idx].qualified_name = strdup(pending_key);
        schema->fields[idx].value          = strdup(pending_default);
        schema->fields[idx].type           = parse_type_string(pending_type);
        schema->fields[idx].required       = pending_required;
        idx++;
    }

    fclose(fp);
    schema->field_count = idx;

    /* 如果未解析到 version，使用默认值 */
    if (!schema->version)
    {
        schema->version = strdup("1.0");
        if (!schema->version)
        {
            bs_schema_free(schema);
            return -1;
        }
    }

    *out = schema;
    return 0;
}

/* ── Gold standard lookup helpers ──────────────────────────────────── */
static const bs_gold_field_def_t* find_gold(const char* qualified_name)
{
    if (!qualified_name)
        return NULL;
    for (size_t i = 0; i < BS_GOLD_STANDARD_V1_COUNT; i++)
    {
        if (strcmp(BS_GOLD_STANDARD_V1[i].qualified_name, qualified_name) == 0)
            return &BS_GOLD_STANDARD_V1[i];
    }
    return NULL;
}

static int is_gold_field(const char* qualified_name)
{
    return find_gold(qualified_name) != NULL;
}

static int is_gold_field_required(const char* qualified_name)
{
    const bs_gold_field_def_t* g = find_gold(qualified_name);
    return g ? g->required : 0;
}

static bs_schema_type_t gold_field_type(const char* qualified_name)
{
    const bs_gold_field_def_t* g = find_gold(qualified_name);
    return g ? g->type : BS_SCHEMA_TYPE_STR;
}
