#include <bs/kernel/schema/schema_json_converter.h>
#include <bs/kernel/schema/schema_types.h>
#include "schema_compat.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Minimal ad-hoc JSON builder + parser. No external dependency.
 */

/* ── Dynamic JSON string builder ───────────────────────────────────── */
typedef struct
{
    char*  buf;
    size_t len;
    size_t cap;
} json_buf_t;

static int json_buf_init(json_buf_t* b)
{
    b->cap = 4096;
    b->buf = (char*)malloc(b->cap);
    if (!b->buf) return -1;
    b->buf[0] = '\0';
    b->len    = 0;
    return 0;
}

static int json_buf_grow(json_buf_t* b, size_t needed)
{
    if (b->len + needed < b->cap) return 0;
    size_t new_cap = b->cap * 2;
    while (b->len + needed >= new_cap) new_cap *= 2;
    char* new_buf = (char*)realloc(b->buf, new_cap);
    if (!new_buf) return -1;
    b->buf    = new_buf;
    b->cap    = new_cap;
    return 0;
}

static int json_buf_append(json_buf_t* b, const char* s)
{
    size_t slen = strlen(s);
    if (json_buf_grow(b, slen + 1)) return -1;
    memcpy(b->buf + b->len, s, slen);
    b->len += slen;
    b->buf[b->len] = '\0';
    return 0;
}

static int json_buf_appendf(json_buf_t* b, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (needed < 0) return -1;
    size_t n = (size_t)needed;
    if (json_buf_grow(b, n + 1)) return -1;
    va_start(args, fmt);
    vsnprintf(b->buf + b->len, b->cap - b->len, fmt, args);
    va_end(args);
    b->len += n;
    b->buf[b->len] = '\0';
    return 0;
}

static void json_buf_destroy(json_buf_t* b)
{
    free(b->buf);
    b->buf = NULL;
}

/* ── JSON string escape ────────────────────────────────────────────── */
static void json_escape(const char* s, json_buf_t* b)
{
    json_buf_append(b, "\"");
    while (*s)
    {
        char c = *s;
        switch (c)
        {
        case '"':  json_buf_append(b, "\\\""); break;
        case '\\': json_buf_append(b, "\\\\"); break;
        case '\n': json_buf_append(b, "\\n"); break;
        case '\r': json_buf_append(b, "\\r"); break;
        case '\t': json_buf_append(b, "\\t"); break;
        default:
            if ((unsigned char)c < 0x20)
                json_buf_appendf(b, "\\u%04x", (unsigned char)c);
            else
                json_buf_appendf(b, "%c", c);
            break;
        }
        s++;
    }
    json_buf_append(b, "\"");
}

/* ── Emit x-blessstar block ────────────────────────────────────────── */
static int emit_x_blessstar(json_buf_t* b, const bs_schema_field_def_t* fd)
{
    int has = (fd->ai_hint || fd->custom_validator ||
               fd->ui_label || fd->ui_description ||
               fd->ui_placeholder || fd->ui_order != 0);
    if (!has) return 0;

    json_buf_append(b, ",\"x-blessstar\":{");
    int first = 1;

    if (fd->ai_hint)
    {
        if (!first) json_buf_append(b, ",");
        json_buf_append(b, "\"ai_hint\":");
        json_escape(fd->ai_hint, b);
        first = 0;
    }
    if (fd->custom_validator)
    {
        if (!first) json_buf_append(b, ",");
        json_buf_append(b, "\"custom_validator\":");
        json_escape(fd->custom_validator, b);
        first = 0;
    }
    if (fd->ui_label || fd->ui_description || fd->ui_placeholder || fd->ui_order != 0)
    {
        if (!first) json_buf_append(b, ",");
        json_buf_append(b, "\"ui\":{");
        int ufirst = 1;
        if (fd->ui_label)
        {
            if (!ufirst) json_buf_append(b, ",");
            json_buf_append(b, "\"label\":");
            json_escape(fd->ui_label, b);
            ufirst = 0;
        }
        if (fd->ui_description)
        {
            if (!ufirst) json_buf_append(b, ",");
            json_buf_append(b, "\"description\":");
            json_escape(fd->ui_description, b);
            ufirst = 0;
        }
        if (fd->ui_placeholder)
        {
            if (!ufirst) json_buf_append(b, ",");
            json_buf_append(b, "\"placeholder\":");
            json_escape(fd->ui_placeholder, b);
            ufirst = 0;
        }
        if (fd->ui_order != 0)
        {
            if (!ufirst) json_buf_append(b, ",");
            json_buf_appendf(b, "\"order\":%d", fd->ui_order);
            ufirst = 0;
        }
        json_buf_append(b, "}");
        first = 0;
    }

    json_buf_append(b, "}");
    return 0;
}

/* ── Emit a single BlessStar field definition to Draft-07 ──────────── */
static int emit_field_to_draft07(json_buf_t* b,
                                  const bs_schema_field_def_t* fd,
                                  int is_root_level,
                                  int required_index)
{
    json_buf_append(b, "{");

    switch (fd->type)
    {
    case BS_SCHEMA_TYPE_STR:
        json_buf_append(b, "\"type\":\"string\""); break;
    case BS_SCHEMA_TYPE_I32:
        json_buf_append(b, "\"type\":\"integer\",\"minimum\":-2147483648,\"maximum\":2147483647"); break;
    case BS_SCHEMA_TYPE_I64:
        json_buf_append(b, "\"type\":\"integer\""); break;
    case BS_SCHEMA_TYPE_F64:
        json_buf_append(b, "\"type\":\"number\""); break;
    case BS_SCHEMA_TYPE_BOOL:
        json_buf_append(b, "\"type\":\"boolean\""); break;
    case BS_SCHEMA_TYPE_ARR:
        json_buf_append(b, "\"type\":\"array\""); break;
    case BS_SCHEMA_TYPE_OBJ:
        json_buf_append(b, "\"type\":\"object\""); break;
    case BS_SCHEMA_TYPE_ENUM:
        json_buf_append(b, "\"type\":\"string\""); break;
    }

    if (fd->range.has_min)
        json_buf_appendf(b, ",\"minimum\":%g", fd->range.min);
    if (fd->range.has_max)
        json_buf_appendf(b, ",\"maximum\":%g", fd->range.max);

    if (fd->pattern)
    {
        json_buf_append(b, ",\"pattern\":");
        json_escape(fd->pattern, b);
    }

    if (fd->enum_values)
    {
        json_buf_append(b, ",\"enum\":[");
        int first = 1;
        for (int i = 0; fd->enum_values[i]; i++)
        {
            if (!first) json_buf_append(b, ",");
            json_escape(fd->enum_values[i], b);
            first = 0;
        }
        json_buf_append(b, "]");
    }

    if (fd->type == BS_SCHEMA_TYPE_OBJ && fd->nested_fields && fd->nested_count > 0)
    {
        json_buf_append(b, ",\"properties\":{");
        for (size_t i = 0; i < fd->nested_count; i++)
        {
            if (i > 0) json_buf_append(b, ",");
            json_escape(fd->nested_fields[i].name, b);
            json_buf_append(b, ":");
            emit_field_to_draft07(b, &fd->nested_fields[i], 0, -1);
        }
        json_buf_append(b,"}");
        json_buf_append(b, ",\"required\":[");
        int rfirst = 1;
        for (size_t i = 0; i < fd->nested_count; i++)
        {
            if (fd->nested_fields[i].required)
            {
                if (!rfirst) json_buf_append(b, ",");
                json_escape(fd->nested_fields[i].name, b);
                rfirst = 0;
            }
        }
        json_buf_append(b, "]");
    }

    if (fd->type == BS_SCHEMA_TYPE_ARR)
    {
        json_buf_append(b, ",\"items\":{");
        bs_schema_type_t et = fd->elem_type;
        switch (et)
        {
        case BS_SCHEMA_TYPE_STR:  json_buf_append(b, "\"type\":\"string\""); break;
        case BS_SCHEMA_TYPE_I32:  json_buf_append(b, "\"type\":\"integer\",\"minimum\":-2147483648,\"maximum\":2147483647"); break;
        case BS_SCHEMA_TYPE_I64:  json_buf_append(b, "\"type\":\"integer\""); break;
        case BS_SCHEMA_TYPE_F64:  json_buf_append(b, "\"type\":\"number\""); break;
        case BS_SCHEMA_TYPE_BOOL: json_buf_append(b, "\"type\":\"boolean\""); break;
        case BS_SCHEMA_TYPE_OBJ:
            json_buf_append(b, "\"type\":\"object\",\"properties\":{");
            if (fd->elem_fields && fd->elem_nested_count > 0)
            {
                for (size_t i = 0; i < fd->elem_nested_count; i++)
                {
                    if (i > 0) json_buf_append(b, ",");
                    json_escape(fd->elem_fields[i].name, b);
                    json_buf_append(b, ":");
                    emit_field_to_draft07(b, &fd->elem_fields[i], 0, -1);
                }
            }
            json_buf_append(b, "}"); break;
        default: json_buf_append(b, "\"type\":\"string\""); break;
        }
        json_buf_append(b, "}");
    }

    emit_x_blessstar(b, fd);
    json_buf_append(b, "}");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * BlessStar -> Draft-07
 * ══════════════════════════════════════════════════════════════════════ */
int bs_json_converter_to_draft07(const bs_schema_entry_t* entry,
                                  char** out_json, size_t* out_len)
{
    if (!entry || !out_json) return BS_SCHEMA_ERR_INVALID_ARG;

    json_buf_t b;
    if (json_buf_init(&b)) return BS_SCHEMA_ERR_NO_MEMORY;

    json_buf_append(&b, "{\n");
    json_buf_appendf(&b, "\"$schema\":\"http://json-schema.org/draft-07/schema#\",\n");
    json_buf_appendf(&b, "\"$id\":\"%s\",\n", entry->schema_id ? entry->schema_id : "unknown");
    json_buf_appendf(&b, "\"title\":\"%s\",\n", entry->ui_meta.title ? entry->ui_meta.title : "");
    json_buf_appendf(&b, "\"description\":\"%s\",\n", entry->ui_meta.description ? entry->ui_meta.description : "");
    json_buf_append(&b, "\"type\":\"object\",\n");

    json_buf_append(&b, "\"properties\":{\n");
    for (size_t i = 0; i < entry->root_count; i++)
    {
        if (i > 0) json_buf_append(&b, ",\n");
        json_escape(entry->root_fields[i].name, &b);
        json_buf_append(&b, ":");
        emit_field_to_draft07(&b, &entry->root_fields[i], 1, -1);
    }
    json_buf_append(&b, "\n},\n");

    json_buf_append(&b, "\"required\":[");
    int rfirst = 1;
    for (size_t i = 0; i < entry->root_count; i++)
    {
        if (entry->root_fields[i].required)
        {
            if (!rfirst) json_buf_append(&b, ",");
            json_escape(entry->root_fields[i].name, &b);
            rfirst = 0;
        }
    }
    json_buf_append(&b, "],\n");

    json_buf_append(&b, "\"x-blessstar\":{");
    json_buf_appendf(&b, "\"schema_id\":\"%s\",\"version\":\"%s\"",
                     entry->schema_id ? entry->schema_id : "",
                     entry->version ? entry->version : "");
    json_buf_append(&b, "}\n}\n");

    *out_json = b.buf;
    if (out_len) *out_len = b.len;
    return BS_SCHEMA_OK;
}

/* ══════════════════════════════════════════════════════════════════════
 * Draft-07 -> BlessStar - minimal tokenizer
 * ══════════════════════════════════════════════════════════════════════ */

typedef enum
{
    DTOK_EOF, DTOK_LBRACE, DTOK_RBRACE, DTOK_LBRACK, DTOK_RBRACK,
    DTOK_COMMA, DTOK_COLON, DTOK_STRING, DTOK_NUMBER,
    DTOK_TRUE, DTOK_FALSE, DTOK_NULL, DTOK_ERROR
} dtok_kind_t;

typedef struct
{
    const char* start;    /* start of input */
    const char* pos;      /* current position */
    const char* tok_start; /* start of current token (first byte = first byte of value) */
    const char* end;
    dtok_kind_t kind;
    char*       str_val;  /* unquoted string content, owned */
    double      num_val;
    char        err_msg[128];
} dparse_t;

static void dparse_init(dparse_t* p, const char* json, size_t len)
{
    p->start = p->pos = json;
    p->end   = json + len;
    p->str_val = NULL;
    p->tok_start = NULL;
}

static void dparse_advance(dparse_t* p)
{
    free(p->str_val);
    p->str_val = NULL;
    p->tok_start = NULL;

    while (p->pos < p->end && (unsigned char)*p->pos <= ' ')
        p->pos++;
    if (p->pos >= p->end) { p->kind = DTOK_EOF; return; }

    p->tok_start = p->pos;
    char c = *p->pos;
    switch (c)
    {
    case '{': p->pos++; p->kind = DTOK_LBRACE; return;
    case '}': p->pos++; p->kind = DTOK_RBRACE; return;
    case '[': p->pos++; p->kind = DTOK_LBRACK; return;
    case ']': p->pos++; p->kind = DTOK_RBRACK; return;
    case ',': p->pos++; p->kind = DTOK_COMMA;  return;
    case ':': p->pos++; p->kind = DTOK_COLON;  return;
    case '"':
    {
        p->pos++;
        const char* sstart = p->pos;
        size_t len = 0;
        while (p->pos < p->end && *p->pos != '"')
        {
            if (*p->pos == '\\' && p->pos + 1 < p->end) p->pos++;
            p->pos++; len++;
        }
        if (p->pos >= p->end) { p->kind = DTOK_ERROR; return; }
        p->str_val = (char*)malloc(len + 1);
        if (p->str_val)
        {
            const char* src = sstart; size_t wi = 0;
            for (size_t i = 0; i < len; i++)
            {
                if (*src == '\\' && *(src+1))
                {
                    src++;
                    switch (*src) {
                    case 'n': p->str_val[wi++] = '\n'; break;
                    case 't': p->str_val[wi++] = '\t'; break;
                    case 'r': p->str_val[wi++] = '\r'; break;
                    case '\\': p->str_val[wi++] = '\\'; break;
                    case '"':  p->str_val[wi++] = '"'; break;
                    default:  p->str_val[wi++] = *src; break;
                    }
                }
                else p->str_val[wi++] = *src;
                src++;
            }
            p->str_val[wi] = '\0';
        }
        p->pos++; p->kind = DTOK_STRING; return;
    }
    default:
        if (c == 't' && p->end - p->pos >= 4 && memcmp(p->pos, "true", 4) == 0)
        { p->pos += 4; p->kind = DTOK_TRUE; return; }
        if (c == 'f' && p->end - p->pos >= 5 && memcmp(p->pos, "false", 5) == 0)
        { p->pos += 5; p->kind = DTOK_FALSE; return; }
        if (c == 'n' && p->end - p->pos >= 4 && memcmp(p->pos, "null", 4) == 0)
        { p->pos += 4; p->kind = DTOK_NULL; return; }
        if (c == '-' || (c >= '0' && c <= '9'))
        {
            char* end; p->num_val = strtod(p->pos, &end);
            if (end > p->pos) { p->pos = end; p->kind = DTOK_NUMBER; return; }
        }
        p->kind = DTOK_ERROR; return;
    }
}

/* ── JSON dict ─────────────────────────────────────────────────────── */
#define MAX_DICT 128
typedef struct
{
    char*  keys[MAX_DICT];
    char*  vals[MAX_DICT]; /* owned JSON substring */
    int    count;
} json_dict_t;

static void json_dict_init(json_dict_t* d) { d->count = 0; }

static void json_dict_add(json_dict_t* d, const char* key,
                           const char* val_data, size_t val_len)
{
    if (d->count >= MAX_DICT) return;
    d->keys[d->count] = bs_strdup(key);
    d->vals[d->count] = (char*)malloc(val_len + 1);
    if (d->vals[d->count])
    {
        memcpy(d->vals[d->count], val_data, val_len);
        d->vals[d->count][val_len] = '\0';
    }
    d->count++;
}

static const char* json_dict_get(const json_dict_t* d, const char* key)
{
    for (int i = 0; i < d->count; i++)
        if (d->keys[i] && strcmp(d->keys[i], key) == 0)
            return d->vals[i];
    return NULL;
}

static void json_dict_destroy(json_dict_t* d)
{
    for (int i = 0; i < d->count; i++)
    { free(d->keys[i]); free(d->vals[i]); }
}

/* ── Parse a dict object (each key:value pair into the dict) ────────── */
/* After call: p is at RBRACE (consumed) */
static void parse_dict_into(dparse_t* p, json_dict_t* out)
{
    if (p->kind != DTOK_LBRACE) return;
    dparse_advance(p); /* skip { */

    while (p->kind == DTOK_STRING)
    {
        char* key = bs_strdup(p->str_val);
        dparse_advance(p);
        if (p->kind != DTOK_COLON) { free(key); return; }
        dparse_advance(p);

        /* Capture value — save pos before consuming */
        const char* val_pos = p->tok_start;
        size_t val_len = 0;

        if (p->kind == DTOK_LBRACE || p->kind == DTOK_LBRACK)
        {
            int depth = 1;
            val_pos = p->tok_start;
            while (depth > 0 && p->kind != DTOK_EOF && p->kind != DTOK_ERROR)
            {
                dparse_advance(p);
                if (p->kind == DTOK_LBRACE || p->kind == DTOK_LBRACK) depth++;
                if (p->kind == DTOK_RBRACE || p->kind == DTOK_RBRACK) depth--;
            }
            val_len = (size_t)(p->pos - val_pos);
            /* Advance past the closing delimiter */
            if (p->kind != DTOK_EOF) dparse_advance(p);
        }
        else
        {
            /* Simple value token — tok_start is the first byte */
            val_pos = p->tok_start;
            dparse_advance(p);
            val_len = (size_t)(p->pos - val_pos);
        }

        json_dict_add(out, key, val_pos, val_len);
        free(key);

        /* After the value, advance past COMMA to next key, or at RBRACE */
        if (p->kind != DTOK_EOF && p->kind != DTOK_RBRACE && p->kind != DTOK_RBRACK)
        {
            if (p->kind == DTOK_COMMA) dparse_advance(p);
        }
    }
}

/* ── Extract a quoted string removing outer quotes ─────────────────── */
static char* extract_quoted(const char* s)
{
    if (!s || s[0] != '"') return NULL;
    size_t sl = strlen(s);
    if (sl < 2) return NULL;
    char* out = (char*)malloc(sl);
    if (!out) return NULL;
    memcpy(out, s + 1, sl - 2);
    out[sl - 2] = '\0';
    return out;
}

/* ── Forward ────────────────────────────────────────────────────────── */
static int parse_draft07_to_entry(const char* json, size_t len,
                                    bs_schema_entry_t* entry);

int bs_json_converter_from_draft07(const char* json_string,
                                    size_t json_len,
                                    bs_schema_entry_t** out_entry)
{
    if (!json_string || !out_entry) return BS_SCHEMA_ERR_INVALID_ARG;
    bs_schema_entry_t* entry = (bs_schema_entry_t*)calloc(1, sizeof(bs_schema_entry_t));
    if (!entry) return BS_SCHEMA_ERR_NO_MEMORY;
    int ret = parse_draft07_to_entry(json_string, json_len, entry);
    if (ret != BS_SCHEMA_OK) { bs_json_converter_free_entry(entry); free(entry); return ret; }
    *out_entry = entry;
    return BS_SCHEMA_OK;
}

void bs_json_converter_free_entry(bs_schema_entry_t* entry)
{
    if (!entry) return;
    free(entry->schema_id);
    free(entry->version);
    bs_schema_field_def_free(entry->root_fields, entry->root_count);
    entry->root_fields = NULL; entry->root_count = 0;
    entry->schema_id = NULL; entry->version = NULL;
}

/* ── Parsing logic ─────────────────────────────────────────────────── */
static int parse_draft07_to_entry(const char* json, size_t len,
                                    bs_schema_entry_t* entry)
{
    dparse_t p;
    dparse_init(&p, json, len);
    dparse_advance(&p);

    if (p.kind != DTOK_LBRACE) return BS_SCHEMA_ERR_PARSE;

    json_dict_t root_dict;
    json_dict_init(&root_dict);
    parse_dict_into(&p, &root_dict);

    /* Extract schema_id */
    const char* sid_q = json_dict_get(&root_dict, "$id");
    entry->schema_id = sid_q ? extract_quoted(sid_q) : NULL;
    if (!entry->schema_id) entry->schema_id = bs_strdup("unknown");
    entry->version = bs_strdup("1.0");

    /* Extract properties */
    const char* props_json = json_dict_get(&root_dict, "properties");
    if (!props_json) { json_dict_destroy(&root_dict); return BS_SCHEMA_OK; }

    dparse_t pp;
    dparse_init(&pp, props_json, strlen(props_json));
    dparse_advance(&pp);

    json_dict_t props_dict;
    json_dict_init(&props_dict);
    parse_dict_into(&pp, &props_dict);

    const char* req_json = json_dict_get(&root_dict, "required");

    entry->root_fields = (bs_schema_field_def_t*)
        calloc(props_dict.count, sizeof(bs_schema_field_def_t));
    entry->root_count = (size_t)props_dict.count;

    for (int i = 0; i < props_dict.count; i++)
    {
        bs_schema_field_def_t* fd = &entry->root_fields[i];
        memset(fd, 0, sizeof(*fd));
        fd->name = props_dict.keys[i];
        props_dict.keys[i] = NULL;

        if (!props_dict.vals[i]) continue;

        dparse_t fp;
        dparse_init(&fp, props_dict.vals[i], strlen(props_dict.vals[i]));
        dparse_advance(&fp);

        if (fp.kind != DTOK_LBRACE) continue;

        json_dict_t field_dict;
        json_dict_init(&field_dict);
        parse_dict_into(&fp, &field_dict);

        /* type */
        const char* ts = json_dict_get(&field_dict, "type");
        if (ts)
        {
            if      (strcmp(ts, "\"string\"")  == 0) fd->type = BS_SCHEMA_TYPE_STR;
            else if (strcmp(ts, "\"integer\"") == 0) fd->type = BS_SCHEMA_TYPE_I64;
            else if (strcmp(ts, "\"number\"")  == 0) fd->type = BS_SCHEMA_TYPE_F64;
            else if (strcmp(ts, "\"boolean\"") == 0) fd->type = BS_SCHEMA_TYPE_BOOL;
            else if (strcmp(ts, "\"array\"")   == 0) fd->type = BS_SCHEMA_TYPE_ARR;
            else if (strcmp(ts, "\"object\"")  == 0) fd->type = BS_SCHEMA_TYPE_OBJ;
        }

        /* range */
        const char* min_s = json_dict_get(&field_dict, "minimum");
        const char* max_s = json_dict_get(&field_dict, "maximum");
        if (min_s) { fd->range.has_min = true; fd->range.min = atof(min_s); }
        if (max_s) { fd->range.has_max = true; fd->range.max = atof(max_s); }

        /* i32 detection */
        if (fd->type == BS_SCHEMA_TYPE_I64 && min_s && max_s &&
            fd->range.min == -2147483648.0 && fd->range.max == 2147483647.0)
            fd->type = BS_SCHEMA_TYPE_I32;

        /* pattern */
        const char* pat_s = json_dict_get(&field_dict, "pattern");
        if (pat_s) fd->pattern = extract_quoted(pat_s);

        /* enum */
        const char* enum_s = json_dict_get(&field_dict, "enum");
        if (enum_s)
        {
            fd->type = BS_SCHEMA_TYPE_ENUM;
            int cnt = 0; const char* ec = enum_s;
            while (*ec) { if (*ec++ == '"') cnt++; }
            const char** ev = (const char**)calloc(cnt + 1, sizeof(char*));
            if (ev)
            {
                int idx = 0; const char* esc = enum_s + 1;
                while (*esc && *esc != ']' && idx < cnt)
                {
                    if (*esc == '"') { esc++; const char* es = esc;
                        while (*esc && *esc != '"') esc++;
                        ev[idx++] = bs_strndup(es, (size_t)(esc - es)); }
                    else esc++;
                }
                ev[idx] = NULL; fd->enum_values = ev;
            }
        }

        /* x-blessstar */
        const char* xb = json_dict_get(&field_dict, "x-blessstar");
        if (xb)
        {
            dparse_t xp;
            dparse_init(&xp, xb, strlen(xb));
            dparse_advance(&xp);

            if (xp.kind == DTOK_LBRACE)
            {
                json_dict_t xdict;
                json_dict_init(&xdict);
                parse_dict_into(&xp, &xdict);

                const char* ah = json_dict_get(&xdict, "ai_hint");
                fd->ai_hint = ah ? extract_quoted(ah) : bs_strdup("");
                if (!fd->ai_hint) fd->ai_hint = bs_strdup("");

                const char* cv = json_dict_get(&xdict, "custom_validator");
                if (cv) fd->custom_validator = extract_quoted(cv);

                const char* ui_s = json_dict_get(&xdict, "ui");
                if (ui_s)
                {
                    dparse_t up;
                    dparse_init(&up, ui_s, strlen(ui_s));
                    dparse_advance(&up);
                    if (up.kind == DTOK_LBRACE)
                    {
                        json_dict_t udict;
                        json_dict_init(&udict);
                        parse_dict_into(&up, &udict);

                        const char* ul = json_dict_get(&udict, "label");
                        if (ul) fd->ui_label = extract_quoted(ul);
                        const char* ud = json_dict_get(&udict, "description");
                        if (ud) fd->ui_description = extract_quoted(ud);
                        const char* uph = json_dict_get(&udict, "placeholder");
                        if (uph) fd->ui_placeholder = extract_quoted(uph);
                        const char* uo = json_dict_get(&udict, "order");
                        if (uo) fd->ui_order = (int)atof(uo);
                        json_dict_destroy(&udict);
                    }
                }
                json_dict_destroy(&xdict);
            }
        }

        json_dict_destroy(&field_dict);
    }

    /* Handle required */
    if (req_json)
    {
        const char* rp = req_json;
        while (*rp)
        {
            if (*rp == '"')
            {
                rp++; const char* rs = rp;
                while (*rp && *rp != '"') rp++;
                size_t rl = (size_t)(rp - rs);
                for (size_t j = 0; j < entry->root_count; j++)
                {
                    if (entry->root_fields[j].name &&
                        strlen(entry->root_fields[j].name) == rl &&
                        memcmp(entry->root_fields[j].name, rs, rl) == 0)
                    { entry->root_fields[j].required = true; break; }
                }
            }
            else rp++;
        }
    }

    json_dict_destroy(&props_dict);
    json_dict_destroy(&root_dict);
    free(p.str_val);
    return BS_SCHEMA_OK;
}
