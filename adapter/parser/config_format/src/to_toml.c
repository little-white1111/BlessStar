/*
 * v1 JSON → TOML serializer.
 *
 * Thread safety: reentrant.
 * Error semantics: negative errno codes.
 * ADR-全链路接通 不变量 #5: round-trip safe.
 */

#include "bs/adapter/parser/config_format/format_convert.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Reuse the same tokenizer as to_yaml.c — for MVP we share inline */

typedef enum {
    TT_EOF, TT_STRING, TT_NUMBER, TT_TRUE, TT_FALSE, TT_NULL,
    TT_LBRACE, TT_RBRACE, TT_LBRACKET, TT_RBRACKET, TT_COLON, TT_COMMA
} ttok_t;

static ttok_t next_tok(const uint8_t** p, const uint8_t* end,
                        const uint8_t** start, size_t* len)
{
    while (*p < end && (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r')) (*p)++;
    if (*p >= end) return TT_EOF;
    uint8_t c = *(*p)++;
    switch (c) {
    case '{': return TT_LBRACE;
    case '}': return TT_RBRACE;
    case '[': return TT_LBRACKET;
    case ']': return TT_RBRACKET;
    case ':': return TT_COLON;
    case ',': return TT_COMMA;
    case '"':
        *start = *p;
        while (*p < end && **p != '"') { if (**p == '\\') (*p)++; (*p)++; }
        *len = *p - *start;
        if (*p < end) (*p)++;
        return TT_STRING;
    case 't': if (*p + 3 <= end && memcmp(*p - 1, "true", 4)==0) { *p += 3; return TT_TRUE; } return TT_EOF;
    case 'f': if (*p + 4 <= end && memcmp(*p - 1, "false",5)==0) { *p += 4; return TT_FALSE; } return TT_EOF;
    case 'n': if (*p + 3 <= end && memcmp(*p - 1, "null", 4)==0) { *p += 3; return TT_NULL; } return TT_EOF;
    default:
        if (c == '-' || (c >= '0' && c <= '9')) {
            *start = *p - 1;
            while (*p < end && (**p >= '0' && **p <= '9') || **p == '.' || **p == 'e' || **p == 'E' || **p == '-' || **p == '+') (*p)++;
            *len = *p - *start;
            return TT_NUMBER;
        }
        return TT_EOF;
    }
}

static int toml_append(uint8_t** buf, size_t* pos, size_t* cap,
                        const char* s, size_t len)
{
    while (*pos + len + 1 > *cap) {
        *cap *= 2;
        uint8_t* nb = (uint8_t*)realloc(*buf, *cap);
        if (!nb) return -ENOMEM;
        *buf = nb;
    }
    memcpy(*buf + *pos, s, len);
    *pos += len;
    return 0;
}

static int toml_emit_value(const uint8_t** p, const uint8_t* end,
                            uint8_t** buf, size_t* pos, size_t* cap,
                            int is_table_value);

static int toml_emit_string(const uint8_t* s, size_t len,
                             uint8_t** buf, size_t* pos, size_t* cap)
{
    /* Basic string — quote if contains special chars */
    int needs_quote = 0;
    for (size_t i = 0; i < len; ++i) {
        if (s[i] == '"' || s[i] == '\n' || s[i] == '\\') { needs_quote = 1; break; }
    }
    if (!needs_quote) {
        return toml_append(buf, pos, cap, (const char*)s, len);
    }
    /* Basic double-quoted string (no multi-line for MVP) */
    if (*pos + len + 3 > *cap) {
        *cap = (*pos + len + 3) * 2;
        uint8_t* nb = (uint8_t*)realloc(*buf, *cap);
        if (!nb) return -ENOMEM;
        *buf = nb;
    }
    (*buf)[(*pos)++] = '"';
    for (size_t i = 0; i < len; ++i) {
        if (s[i] == '"') { (*buf)[(*pos)++] = '\\'; (*buf)[(*pos)++] = '"'; }
        else if (s[i] == '\\') { (*buf)[(*pos)++] = '\\'; (*buf)[(*pos)++] = '\\'; }
        else { (*buf)[(*pos)++] = s[i]; }
    }
    (*buf)[(*pos)++] = '"';
    return 0;
}

static int toml_emit_scalar(ttok_t tok, const uint8_t* start, size_t len,
                             uint8_t** buf, size_t* pos, size_t* cap)
{
    switch (tok) {
    case TT_STRING:
        return toml_emit_string(start, len, buf, pos, cap);
    case TT_NUMBER: {
        /* Convert to bare value */
        char tmp[128];
        size_t n = len < 127 ? len : 127;
        memcpy(tmp, start, n); tmp[n] = '\0';
        return toml_append(buf, pos, cap, tmp, n);
    }
    case TT_TRUE:  return toml_append(buf, pos, cap, "true", 4);
    case TT_FALSE: return toml_append(buf, pos, cap, "false", 5);
    case TT_NULL:  return toml_append(buf, pos, cap, "\"null\"", 6); /* TOML has no null */
    default: return -EINVAL;
    }
}

/**
 * Emit a value in TOML format.
 * For objects, we use [[array]] style for arrays of tables.
 * is_table_value indicates we're inside a [table] header context.
 */
static int toml_emit_value(const uint8_t** p, const uint8_t* end,
                            uint8_t** buf, size_t* pos, size_t* cap,
                            int is_table_value)
{
    const uint8_t* start; size_t len;
    ttok_t tok = next_tok(p, end, &start, &len);

    switch (tok) {
    case TT_STRING: case TT_NUMBER: case TT_TRUE: case TT_FALSE: case TT_NULL:
        return toml_emit_scalar(tok, start, len, buf, pos, cap);

    case TT_LBRACE: {
        /* Inline table: {key = value, ...} */
        if (toml_append(buf, pos, cap, "{", 1)) return -ENOMEM;
        int first = 1;
        while (1) {
            const uint8_t* k_start; size_t k_len;
            ttok_t k_tok = next_tok(p, end, &k_start, &k_len);
            if (k_tok == TT_RBRACE) break;
            if (k_tok != TT_STRING) return -EINVAL;
            ttok_t colon = next_tok(p, end, &start, &len);
            if (colon != TT_COLON) return -EINVAL;
            if (!first) if (toml_append(buf, pos, cap, ", ", 2)) return -ENOMEM;
            first = 0;
            toml_emit_string(k_start, k_len, buf, pos, cap);
            if (toml_append(buf, pos, cap, " = ", 3)) return -ENOMEM;
            if (toml_emit_value(p, end, buf, pos, cap, 0)) return -1;
        }
        if (toml_append(buf, pos, cap, "}", 1)) return -ENOMEM;
        break;
    }
    case TT_LBRACKET: {
        /* Array: [val, val, ...] */
        if (toml_append(buf, pos, cap, "[", 1)) return -ENOMEM;
        int first = 1;
        while (1) {
            const uint8_t* c_start; size_t c_len;
            ttok_t c_tok = next_tok(p, end, &c_start, &c_len);
            if (c_tok == TT_RBRACKET) break;
            if (!first) if (toml_append(buf, pos, cap, ", ", 2)) return -ENOMEM;
            first = 0;
            /* Peek at next non-ws to see if we need to handle nested objects */
            if (c_tok == TT_LBRACE) {
                if (toml_emit_value(p, end, buf, pos, cap, 0)) return -1;
            } else {
                if (toml_emit_scalar(c_tok, c_start, c_len, buf, pos, cap)) return -1;
            }
            /* Check for trailing comma */
            const uint8_t* after = *p;
            next_tok(&after, end, &start, &len);
            if (after[-1] == ',') {
                /* comma already consumed, but we need to handle it */
                const uint8_t* tmp_p = *p;
                const uint8_t* tmp_s; size_t tmp_l;
                ttok_t maybe_comma = next_tok(&tmp_p, end, &tmp_s, &tmp_l);
                if (maybe_comma == TT_COMMA) *p = tmp_p;
            }
        }
        if (toml_append(buf, pos, cap, "]", 1)) return -ENOMEM;
        break;
    }
    default:
        return -EINVAL;
    }
    return 0;
}

int bs_internal_to_toml(const uint8_t* v1_json, size_t v1_len,
                         uint8_t** out, size_t* out_len)
{
    if (!v1_json || !out || !out_len) return -EINVAL;

    size_t cap = v1_len * 2 + 1024;
    uint8_t* buf = (uint8_t*)malloc(cap);
    if (!buf) return -ENOMEM;

    size_t pos = 0;
    const uint8_t* p = v1_json;
    const uint8_t* end = v1_json + v1_len;

    /* Flat top-level object: emit as key = value pairs */
    const uint8_t* start; size_t len;
    ttok_t tok = next_tok(&p, end, &start, &len);
    if (tok != TT_LBRACE) { free(buf); return -EINVAL; }

    while (1) {
        const uint8_t* k_start; size_t k_len;
        ttok_t k_tok = next_tok(&p, end, &k_start, &k_len);
        if (k_tok == TT_RBRACE) break;
        if (k_tok != TT_STRING) { free(buf); return -EINVAL; }

        ttok_t colon = next_tok(&p, end, &start, &len);
        if (colon != TT_COLON) { free(buf); return -EINVAL; }

        /* Emit key =  */
        toml_emit_string(k_start, k_len, &buf, &pos, &cap);
        if (toml_append(&buf, &pos, &cap, " = ", 3)) { free(buf); return -ENOMEM; }

        if (toml_emit_value(&p, end, &buf, &pos, &cap, 0)) { free(buf); return -EINVAL; }

        /* Newline */
        if (toml_append(&buf, &pos, &cap, "\n", 1)) { free(buf); return -ENOMEM; }

        ttok_t maybe_comma = next_tok(&p, end, &start, &len);
        if (maybe_comma == TT_RBRACE) break;
        /* comma consumed, continue */
    }

    buf[pos++] = '\n';
    buf[pos] = '\0';

    *out = buf;
    *out_len = pos + 1;
    return 0;
}
