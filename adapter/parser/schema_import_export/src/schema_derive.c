/*
 * Heuristic schema derivation from v1 JSON config values.
 *
 * ADR-全链路接通 — 方案C/A: 启发式推导作为底层快速通道。
 * Walks the JSON tree and infers field types.
 *
 * Thread safety: reentrant.
 * Error semantics: negative errno codes.
 */

#include "bs/adapter/parser/schema_import_export/schema_derive.h"

#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── Simple JSON tokenizer (shared pattern with format converters) ─── */

typedef enum {
    ST_EOF, ST_STRING, ST_NUMBER, ST_TRUE, ST_FALSE, ST_NULL,
    ST_LBRACE, ST_RBRACE, ST_LBRACKET, ST_RBRACKET, ST_COLON, ST_COMMA
} stok_t;

static stok_t next_stok(const uint8_t** p, const uint8_t* end,
                         const uint8_t** start, size_t* len)
{
    while (*p < end && (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r')) (*p)++;
    if (*p >= end) return ST_EOF;
    uint8_t c = *(*p)++;
    switch (c) {
    case '{': return ST_LBRACE;
    case '}': return ST_RBRACE;
    case '[': return ST_LBRACKET;
    case ']': return ST_RBRACKET;
    case ':': return ST_COLON;
    case ',': return ST_COMMA;
    case '"':
        *start = *p;
        while (*p < end && **p != '"') { if (**p == '\\') (*p)++; (*p)++; }
        *len = *p - *start;
        if (*p < end) (*p)++;
        return ST_STRING;
    case 't': if (*p + 3 <= end && memcmp(*p-1,"true",4)==0) { *p += 3; return ST_TRUE; } return ST_EOF;
    case 'f': if (*p + 4 <= end && memcmp(*p-1,"false",5)==0) { *p += 4; return ST_FALSE; } return ST_EOF;
    case 'n': if (*p + 3 <= end && memcmp(*p-1,"null",4)==0) { *p += 3; return ST_NULL; } return ST_EOF;
    default:
        if (c == '-' || (c >= '0' && c <= '9')) {
            *start = *p - 1;
            while (*p < end && (**p >= '0' && **p <= '9') || **p == '.' || **p == 'e' || **p == 'E' || **p == '-' || **p == '+') (*p)++;
            *len = *p - *start;
            return ST_NUMBER;
        }
        return ST_EOF;
    }
}

/* ─── Inference helpers ────────────────────────────────────────────── */

static int has_decimal(const uint8_t* s, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        if (s[i] == '.' || s[i] == 'e' || s[i] == 'E') return 1;
    }
    return 0;
}

static int looks_like_bool(const uint8_t* s, size_t len)
{
    return (len == 4 && memcmp(s, "true", 4) == 0) ||
           (len == 5 && memcmp(s, "false", 5) == 0);
}

static int looks_like_enum_candidate(const char* key, const uint8_t* val, size_t val_len)
{
    /* Short string values (< 20 chars) with no whitespace are enum candidates */
    if (val_len > 20 || val_len == 0) return 0;
    for (size_t i = 0; i < val_len; ++i) {
        if (isspace(val[i])) return 0;
    }
    /* Avoid generic keys — likely unique identifiers */
    if (strstr(key, "id") || strstr(key, "name") || strstr(key, "path")) return 0;
    return 1;
}

/* ─── Field array management ───────────────────────────────────────── */

typedef struct field_array {
    bs_derived_field_t* fields;
    size_t count;
    size_t cap;
} field_array_t;

static int fa_init(field_array_t* fa)
{
    fa->cap   = 64;
    fa->count = 0;
    fa->fields = (bs_derived_field_t*)calloc(fa->cap, sizeof(bs_derived_field_t));
    return fa->fields ? 0 : -ENOMEM;
}

static int fa_add(field_array_t* fa, const char* key, bs_derived_type_t type,
                   const uint8_t* val, size_t val_len, int is_required)
{
    if (fa->count >= fa->cap) {
        fa->cap *= 2;
        bs_derived_field_t* tmp = (bs_derived_field_t*)realloc(
            fa->fields, fa->cap * sizeof(bs_derived_field_t));
        if (!tmp) return -ENOMEM;
        fa->fields = tmp;
        memset(fa->fields + fa->count, 0,
               (fa->cap - fa->count) * sizeof(bs_derived_field_t));
    }

    bs_derived_field_t* f = &fa->fields[fa->count];
    f->key = strdup(key);
    if (!f->key) return -ENOMEM;
    f->type = type;
    f->is_required = is_required;

    if (val && val_len > 0) {
        f->example_value = (char*)malloc(val_len + 3);
        if (f->example_value) {
            f->example_value[0] = '"';
            memcpy(f->example_value + 1, val, val_len);
            f->example_value[val_len + 1] = '"';
            f->example_value[val_len + 2] = '\0';
        }
    }

    f->enum_candidate = (type == BS_DTYPE_STRING &&
                         looks_like_enum_candidate(key, val, val_len)) ? 1 : 0;

    fa->count++;
    return 0;
}

static void fa_free(field_array_t* fa)
{
    for (size_t i = 0; i < fa->count; ++i) {
        free(fa->fields[i].key);
        free(fa->fields[i].example_value);
    }
    free(fa->fields);
    fa->fields = NULL;
    fa->count = fa->cap = 0;
}

/* ─── Recursive derivation ─────────────────────────────────────────── */

static int derive_object(const uint8_t** p, const uint8_t* end,
                          field_array_t* fa, const char* prefix)
{
    const uint8_t* start; size_t len;
    while (1) {
        stok_t k_tok = next_stok(p, end, &start, &len);
        if (k_tok == ST_RBRACE) break;
        if (k_tok != ST_STRING) return -EINVAL;

        /* Build full key path */
        char full_key[1024];
        if (prefix && strlen(prefix) > 0) {
            snprintf(full_key, sizeof(full_key), "%s.%.*s", prefix, (int)len, start);
        } else {
            snprintf(full_key, sizeof(full_key), "%.*s", (int)len, start);
        }

        stok_t colon = next_stok(p, end, &start, &len);
        if (colon != ST_COLON) return -EINVAL;

        /* Peek at value type */
        const uint8_t* v_start; size_t v_len;
        stok_t v_tok = next_stok(p, end, &v_start, &v_len);

        switch (v_tok) {
        case ST_STRING:
            fa_add(fa, full_key, BS_DTYPE_STRING, v_start, v_len, 1);
            break;
        case ST_NUMBER:
            fa_add(fa, full_key,
                   has_decimal(v_start, v_len) ? BS_DTYPE_NUMBER : BS_DTYPE_INTEGER,
                   v_start, v_len, 1);
            break;
        case ST_TRUE: case ST_FALSE:
            fa_add(fa, full_key, BS_DTYPE_BOOLEAN, v_start, v_len, 1);
            break;
        case ST_NULL:
            fa_add(fa, full_key, BS_DTYPE_UNKNOWN, NULL, 0, 0);
            break;
        case ST_LBRACE: {
            fa_add(fa, full_key, BS_DTYPE_OBJECT, NULL, 0, 1);
            int rc = derive_object(p, end, fa, full_key);
            if (rc) return rc;
            break;
        }
        case ST_LBRACKET:
            fa_add(fa, full_key, BS_DTYPE_ARRAY, NULL, 0, 1);
            /* Skip array contents for schema derivation (MVP) */
            {
                int depth = 1;
                while (depth > 0 && *p < end) {
                    uint8_t c = *(*p)++;
                    if (c == '[') depth++;
                    else if (c == ']') depth--;
                    else if (c == '"') {
                        while (*p < end && **p != '"') { if (**p == '\\') (*p)++; (*p)++; }
                        if (*p < end) (*p)++;
                    }
                }
            }
            break;
        default:
            return -EINVAL;
        }

        stok_t comma = next_stok(p, end, &start, &len);
        if (comma == ST_RBRACE) break;
        /* comma consumed, continue */
    }
    return 0;
}

/* ─── JSON Schema generation ───────────────────────────────────────── */

static const char* type_to_jsonschema(bs_derived_type_t t)
{
    switch (t) {
    case BS_DTYPE_STRING:  return "\"type\": \"string\"";
    case BS_DTYPE_INTEGER: return "\"type\": \"integer\"";
    case BS_DTYPE_NUMBER:  return "\"type\": \"number\"";
    case BS_DTYPE_BOOLEAN: return "\"type\": \"boolean\"";
    case BS_DTYPE_ARRAY:   return "\"type\": \"array\"";
    case BS_DTYPE_OBJECT:  return "\"type\": \"object\"";
    case BS_DTYPE_ENUM:    return "\"type\": \"string\"";
    default:               return "\"type\": \"string\"";
    }
}

static char* generate_jsonschema(field_array_t* fa)
{
    /* Build a simple flat JSON Schema: properties/{field}: {type} */
    size_t cap = 4096;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;

    int n = snprintf(buf, cap,
        "{\n"
        "  \"$schema\": \"http://json-schema.org/draft-07/schema#\",\n"
        "  \"type\": \"object\",\n"
        "  \"properties\": {\n");

    for (size_t i = 0; i < fa->count; ++i) {
        bs_derived_field_t* f = &fa->fields[i];
        /* Skip nested object keys (already represented by their container) */
        if (f->type == BS_DTYPE_OBJECT || f->type == BS_DTYPE_ARRAY) continue;

        size_t remaining = cap - n;
        int written = snprintf(buf + n, remaining,
            "    \"%s\": { %s }%s\n",
            f->key, type_to_jsonschema(f->type),
            (i < fa->count - 1) ? "," : "");
        if (written < 0 || (size_t)written >= remaining) {
            /* Buffer too small — truncate */
            break;
        }
        n += written;
    }

    n += snprintf(buf + n, cap - n,
        "  },\n"
        "  \"required\": [");

    int first = 1;
    for (size_t i = 0; i < fa->count; ++i) {
        if (fa->fields[i].is_required && fa->fields[i].type != BS_DTYPE_OBJECT && fa->fields[i].type != BS_DTYPE_ARRAY) {
            n += snprintf(buf + n, cap - n, "%s\"%s\"", first ? "" : ", ", fa->fields[i].key);
            first = 0;
        }
    }
    n += snprintf(buf + n, cap - n, "]\n}\n");

    return buf;
}

/* ─── Public API ───────────────────────────────────────────────────── */

int bs_schema_derive(const uint8_t* v1_json, size_t v1_len,
                     bs_derive_result_t* out)
{
    if (!v1_json || !out) return -EINVAL;
    memset(out, 0, sizeof(*out));

    field_array_t fa;
    int rc = fa_init(&fa);
    if (rc) return rc;

    const uint8_t* p = v1_json;
    const uint8_t* end = v1_json + v1_len;

    stok_t tok = next_stok(&p, end, NULL, NULL);
    if (tok == ST_LBRACE) {
        rc = derive_object(&p, end, &fa, "");
    } else if (tok == ST_LBRACKET) {
        /* Top-level array — derive just the array type */
        rc = fa_add(&fa, "$root", BS_DTYPE_ARRAY, NULL, 0, 1);
    } else {
        rc = -EINVAL;
    }

    if (rc) {
        fa_free(&fa);
        return rc;
    }

    char* schema_json = generate_jsonschema(&fa);
    if (!schema_json) {
        fa_free(&fa);
        return -ENOMEM;
    }

    out->fields      = fa.fields;
    out->count       = fa.count;
    out->schema_json = schema_json;
    return 0;
}

void bs_schema_derive_free(bs_derive_result_t* result)
{
    if (!result) return;
    for (size_t i = 0; i < result->count; ++i) {
        free(result->fields[i].key);
        free(result->fields[i].example_value);
    }
    free(result->fields);
    free(result->schema_json);
    memset(result, 0, sizeof(*result));
}
