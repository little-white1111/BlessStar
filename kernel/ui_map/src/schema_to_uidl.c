#include <bs/kernel/ui_map/schema_to_uidl.h>
#include <bs/kernel/ui_map/ui_render_desc.h>
#include <bs/kernel/schema/schema_types.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Schema -> UIDL JSON converter.
 *
 * Reads BlessStar Compact Schema (bs_schema_entry_t / bs_schema_field_def_t),
 * outputs UIDL JSON string.
 *
 * Uses sprintf-based JSON building (structure is fixed and simple).
 */

/* ── Dynamic JSON string builder (local copy, no external dep) ─────── */
typedef struct
{
    char*  buf;
    size_t len;
    size_t cap;
} ujson_buf_t;

static int ujson_init(ujson_buf_t* b)
{
    b->cap = 4096;
    b->buf = (char*)malloc(b->cap);
    if (!b->buf) return -1;
    b->buf[0] = '\0';
    b->len    = 0;
    return 0;
}

static int ujson_grow(ujson_buf_t* b, size_t needed)
{
    if (b->len + needed < b->cap) return 0;
    size_t new_cap = b->cap * 2;
    while (b->len + needed >= new_cap) new_cap *= 2;
    char* nb = (char*)realloc(b->buf, new_cap);
    if (!nb) return -1;
    b->buf = nb;
    b->cap = new_cap;
    return 0;
}

static int ujson_append(ujson_buf_t* b, const char* s)
{
    size_t sl = strlen(s);
    if (ujson_grow(b, sl + 1)) return -1;
    memcpy(b->buf + b->len, s, sl);
    b->len += sl;
    b->buf[b->len] = '\0';
    return 0;
}

static int ujson_appendf(ujson_buf_t* b, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) return -1;
    size_t n = (size_t)needed;
    if (ujson_grow(b, n + 1)) return -1;
    va_start(ap, fmt);
    vsnprintf(b->buf + b->len, b->cap - b->len, fmt, ap);
    va_end(ap);
    b->len += n;
    b->buf[b->len] = '\0';
    return 0;
}

static void ujson_destroy(ujson_buf_t* b)
{
    free(b->buf);
    b->buf = NULL;
}

/* ── JSON string escape ────────────────────────────────────────────── */
static void ujson_escape(ujson_buf_t* b, const char* s)
{
    if (!s) { ujson_append(b, "null"); return; }
    ujson_append(b, "\"");
    while (*s)
    {
        char c = *s;
        switch (c)
        {
        case '"':  ujson_append(b, "\\\""); break;
        case '\\': ujson_append(b, "\\\\"); break;
        case '\n': ujson_append(b, "\\n"); break;
        case '\r': ujson_append(b, "\\r"); break;
        case '\t': ujson_append(b, "\\t"); break;
        default:
            if ((unsigned char)c < 0x20)
                ujson_appendf(b, "\\u%04x", (unsigned char)c);
            else
                ujson_appendf(b, "%c", c);
            break;
        }
        s++;
    }
    ujson_append(b, "\"");
}

/* ── Forward declaration ───────────────────────────────────────────── */
static int emit_controls(ujson_buf_t* b,
                          const bs_schema_field_def_t* fields, size_t count,
                          const char* parent_path);

/* ── Emit single control from schema field ─────────────────────────── */
static int emit_one_control(ujson_buf_t* b,
                             const bs_schema_field_def_t* fd,
                             const char* parent_path)
{
    /* Build dot-notation field path */
    char path_buf[1024];
    if (parent_path && parent_path[0])
        snprintf(path_buf, sizeof(path_buf), "%s.%s", parent_path, fd->name);
    else
        snprintf(path_buf, sizeof(path_buf), "%s", fd->name ? fd->name : "");

    int has_enum = (fd->enum_values && fd->enum_values[0]) ? 1 : 0;
    bs_ui_widget_type_t w = bs_ui_default_widget_for_type((int)fd->type, has_enum);

    ujson_append(b, "{\n");

    /* field (dot-notation) */
    ujson_append(b, "\"field\":");
    ujson_escape(b, path_buf);
    ujson_append(b, ",\n");

    /* widget */
    ujson_append(b, "\"widget\":");
    ujson_escape(b, bs_ui_widget_type_str(w));
    ujson_append(b, ",\n");

    /* label */
    if (fd->ui_label)
    {
        ujson_append(b, "\"label\":");
        ujson_escape(b, fd->ui_label);
        ujson_append(b, ",\n");
    }

    /* order */
    ujson_appendf(b, "\"order\":%d,\n", fd->ui_order);

    /* ai_layout_hint — direct passthrough from schema field's ai_hint (UIDL-02) */
    if (fd->ai_hint)
    {
        ujson_append(b, "\"ai_layout_hint\":");
        ujson_escape(b, fd->ai_hint);
        ujson_append(b, ",\n");
    }

    /* visibility — kernel does NOT process (UIDL-03), always null */
    ujson_append(b, "\"visibility\":null,\n");

    /* default_value */
    ujson_append(b, "\"default_value\":null,\n");

    /* children (nested for object types) */
    if (fd->type == BS_SCHEMA_TYPE_OBJ && fd->nested_fields && fd->nested_count > 0)
    {
        ujson_append(b, "\"children\":[\n");
        emit_controls(b, fd->nested_fields, fd->nested_count, path_buf);
        ujson_append(b, "],\n");
    }
    else if (fd->type == BS_SCHEMA_TYPE_ARR && fd->elem_type == BS_SCHEMA_TYPE_OBJ
             && fd->elem_fields && fd->elem_nested_count > 0)
    {
        /* Array of objects — emit element fields as children */
        ujson_append(b, "\"children\":[\n");
        emit_controls(b, fd->elem_fields, fd->elem_nested_count, path_buf);
        ujson_append(b, "],\n");
    }
    else
    {
        ujson_append(b, "\"children\":[],\n");
    }

    /* validation_ref — not used in MVP, null */
    ujson_append(b, "\"validation_ref\":null\n");

    ujson_append(b, "}");
    return 0;
}

/* ── Emit array of controls ────────────────────────────────────────── */
static int emit_controls(ujson_buf_t* b,
                          const bs_schema_field_def_t* fields, size_t count,
                          const char* parent_path)
{
    for (size_t i = 0; i < count; i++)
    {
        if (i > 0) ujson_append(b, ",\n");
        emit_one_control(b, &fields[i], parent_path);
    }
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Public API: Schema entry -> UIDL JSON string
 * ══════════════════════════════════════════════════════════════════════ */
int bs_schema_to_uidl(const bs_schema_entry_t* entry,
                       char** out_json, size_t* out_len)
{
    if (!entry || !out_json) return -1; /* BS_SCHEMA_ERR_INVALID_ARG */

    ujson_buf_t b;
    if (ujson_init(&b)) return -8; /* BS_SCHEMA_ERR_NO_MEMORY */

    ujson_append(&b, "{\n");

    /* uidl_version */
    ujson_append(&b, "\"uidl_version\":1,\n");

    /* schema_ref */
    ujson_append(&b, "\"schema_ref\":");
    ujson_escape(&b, entry->schema_id ? entry->schema_id : "unknown");
    ujson_append(&b, ",\n");

    /* controls array */
    ujson_append(&b, "\"controls\":[\n");
    if (entry->root_fields && entry->root_count > 0)
        emit_controls(&b, entry->root_fields, entry->root_count, "");
    ujson_append(&b, "]\n");

    ujson_append(&b, "}\n");

    *out_json = b.buf;
    if (out_len) *out_len = b.len;
    return 0;
}
