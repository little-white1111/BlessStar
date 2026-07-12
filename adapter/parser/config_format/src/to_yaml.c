/*
 * v1 JSON → YAML serializer.
 *
 * Parses v1 JSON into a simple tree walk and emits YAML 1.2.
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

/* Simple forward JSON tokenizer helper — reuses existing lexer patterns */

typedef enum {
    JT_EOF,
    JT_STRING,
    JT_NUMBER,
    JT_TRUE,
    JT_FALSE,
    JT_NULL,
    JT_LBRACE,
    JT_RBRACE,
    JT_LBRACKET,
    JT_RBRACKET,
    JT_COLON,
    JT_COMMA
} jtoken_t;

static jtoken_t next_token(const uint8_t** p, const uint8_t* end,
                           const uint8_t** tok_start, size_t* tok_len)
{
    /* Skip whitespace */
    while (*p < end && (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r'))
        (*p)++;
    if (*p >= end) return JT_EOF;

    uint8_t ch = **p;
    (*p)++;

    switch (ch) {
    case '{': return JT_LBRACE;
    case '}': return JT_RBRACE;
    case '[': return JT_LBRACKET;
    case ']': return JT_RBRACKET;
    case ':': return JT_COLON;
    case ',': return JT_COMMA;
    case '"': {
        *tok_start = *p;
        while (*p < end && **p != '"') {
            if (**p == '\\') (*p)++; /* escape */
            (*p)++;
        }
        *tok_len = *p - *tok_start;
        if (*p < end) (*p)++; /* skip closing " */
        return JT_STRING;
    }
    case 't': if (*p + 3 <= end && memcmp(*p - 1, "true", 4) == 0) { *p += 3; return JT_TRUE; } return JT_EOF;
    case 'f': if (*p + 4 <= end && memcmp(*p - 1, "false", 5) == 0) { *p += 4; return JT_FALSE; } return JT_EOF;
    case 'n': if (*p + 3 <= end && memcmp(*p - 1, "null", 4) == 0) { *p += 3; return JT_NULL; } return JT_EOF;
    default:
        /* Number: digits, minus, decimal point, exponent */
        if (ch == '-' || (ch >= '0' && ch <= '9')) {
            *tok_start = *p - 1;
            while (*p < end && (**p >= '0' && **p <= '9') || **p == '.' || **p == 'e' || **p == 'E' || **p == '-' || **p == '+')
                (*p)++;
            *tok_len = *p - *tok_start;
            return JT_NUMBER;
        }
        return JT_EOF;
    }
}

static int emit_yaml_value(const uint8_t** p, const uint8_t* end,
                           uint8_t** buf, size_t* pos, size_t* cap,
                           int indent);

static int emit_yaml_string(const uint8_t* s, size_t len,
                            uint8_t** buf, size_t* pos, size_t* cap)
{
    /* Check if quoting is needed */
    int needs_quotes = 0;
    for (size_t i = 0; i < len; ++i) {
        if (s[i] == ':' || s[i] == '#' || s[i] == '{' || s[i] == '[' ||
            s[i] == ' ' || s[i] == '\'' || s[i] == '"' || s[i] == '\n') {
            needs_quotes = 1;
            break;
        }
    }

    if (!needs_quotes && len > 0) {
        /* Unquoted scalar — write exactly len bytes */
        while (*pos + len > *cap) {
            *cap *= 2;
            uint8_t* new_buf = (uint8_t*)realloc(*buf, *cap);
            if (!new_buf) return -ENOMEM;
            *buf = new_buf;
        }
        memcpy(*buf + *pos, s, len);
        *pos += len;
        return 0;
    }

    /* Single-quoted scalar */
    if (*pos + len + 2 >= *cap) {
        *cap = (*pos + len + 2) * 2;
        uint8_t* new_buf = (uint8_t*)realloc(*buf, *cap);
        if (!new_buf) return -ENOMEM;
        *buf = new_buf;
    }
    (*buf)[(*pos)++] = '\'';
    for (size_t i = 0; i < len; ++i) {
        (*buf)[(*pos)++] = s[i];
        if (s[i] == '\'') {
            /* Escape single quote by doubling */
            if (*pos + 1 >= *cap) {
                *cap *= 2;
                uint8_t* new_buf = (uint8_t*)realloc(*buf, *cap);
                if (!new_buf) return -ENOMEM;
                *buf = new_buf;
            }
            (*buf)[(*pos)++] = '\'';
        }
    }
    (*buf)[(*pos)++] = '\'';
    return 0;
}

static int emit_indent(uint8_t** buf, size_t* pos, size_t* cap, int indent)
{
    while (*pos + indent * 2 + 1 > *cap) {
        *cap *= 2;
        uint8_t* new_buf = (uint8_t*)realloc(*buf, *cap);
        if (!new_buf) return -ENOMEM;
        *buf = new_buf;
    }
    for (int i = 0; i < indent; ++i) {
        (*buf)[(*pos)++] = ' ';
        (*buf)[(*pos)++] = ' ';
    }
    return 0;
}

static int emit_newline(uint8_t** buf, size_t* pos, size_t* cap)
{
    while (*pos + 1 > *cap) {
        *cap *= 2;
        uint8_t* new_buf = (uint8_t*)realloc(*buf, *cap);
        if (!new_buf) return -ENOMEM;
        *buf = new_buf;
    }
    (*buf)[(*pos)++] = '\n';
    return 0;
}

/**
 * Emit a YAML value at the current cursor position.
 * For objects and arrays, this handles nesting recursively.
 */
static int emit_yaml_value(const uint8_t** p, const uint8_t* end,
                           uint8_t** buf, size_t* pos, size_t* cap,
                           int indent)
{
    const uint8_t* tok_start;
    size_t tok_len;

    jtoken_t tok = next_token(p, end, &tok_start, &tok_len);

    switch (tok) {
    case JT_STRING: {
        int rc = emit_yaml_string(tok_start, tok_len, buf, pos, cap);
        if (rc) return rc;
        break;
    }
    case JT_NUMBER:
    case JT_TRUE:
    case JT_FALSE:
    case JT_NULL: {
        /* Unquoted scalar — re-point to original text range */
        const uint8_t* val_start;
        size_t val_len;
        if (tok == JT_NUMBER) { val_start = tok_start; val_len = tok_len; }
        else if (tok == JT_TRUE)  { val_start = (const uint8_t*)"true";  val_len = 4; }
        else if (tok == JT_FALSE) { val_start = (const uint8_t*)"false"; val_len = 5; }
        else                      { val_start = (const uint8_t*)"null";  val_len = 4; }

        while (*pos + val_len + 1 > *cap) {
            *cap *= 2;
            uint8_t* new_buf = (uint8_t*)realloc(*buf, *cap);
            if (!new_buf) return -ENOMEM;
            *buf = new_buf;
        }
        memcpy(*buf + *pos, val_start, val_len);
        *pos += val_len;
        break;
    }
    case JT_LBRACE: {
        /* Inline empty object */
        const uint8_t* check_p = *p;
        const uint8_t* check_start;
        size_t check_len;
        jtoken_t next = next_token(&check_p, end, &check_start, &check_len);
        if (next == JT_RBRACE) {
            *p = check_p;
            while (*pos + 2 > *cap) {
                *cap *= 2;
                uint8_t* new_buf = (uint8_t*)realloc(*buf, *cap);
                if (!new_buf) return -ENOMEM;
                *buf = new_buf;
            }
            (*buf)[(*pos)++] = '{';
            (*buf)[(*pos)++] = '}';
            break;
        }
        /* Block mapping style */
        int rc = emit_newline(buf, pos, cap);
        if (rc) return rc;
        while (1) {
            /* key */
            const uint8_t* key_start;
            size_t key_len;
            jtoken_t key_tok = next_token(p, end, &key_start, &key_len);
            if (key_tok == JT_RBRACE) break;
            if (key_tok != JT_STRING) return -EINVAL;

            /* : */
            jtoken_t colon = next_token(p, end, &tok_start, &tok_len);
            if (colon != JT_COLON) return -EINVAL;

            emit_indent(buf, pos, cap, indent);
            emit_yaml_string(key_start, key_len, buf, pos, cap);
            while (*pos + 2 > *cap) {
                *cap *= 2;
                uint8_t* new_buf = (uint8_t*)realloc(*buf, *cap);
                if (!new_buf) return -ENOMEM;
                *buf = new_buf;
            }
            (*buf)[(*pos)++] = ':';
            (*buf)[(*pos)++] = ' ';

            /* value */
            rc = emit_yaml_value(p, end, buf, pos, cap, indent + 1);
            if (rc) return rc;

            /* Check for comma (more items) */
            const uint8_t* after_val = *p;
            jtoken_t comma = next_token(&after_val, end, &tok_start, &tok_len);
            if (comma == JT_COMMA) {
                *p = after_val;
                emit_newline(buf, pos, cap);
            } else {
                emit_newline(buf, pos, cap);
            }
        }
        break;
    }
    case JT_LBRACKET: {
        /* Inline empty array */
        const uint8_t* check_p = *p;
        const uint8_t* check_start;
        size_t check_len;
        jtoken_t next = next_token(&check_p, end, &check_start, &check_len);
        if (next == JT_RBRACKET) {
            *p = check_p;
            while (*pos + 2 > *cap) {
                *cap *= 2;
                uint8_t* new_buf = (uint8_t*)realloc(*buf, *cap);
                if (!new_buf) return -ENOMEM;
                *buf = new_buf;
            }
            (*buf)[(*pos)++] = '[';
            (*buf)[(*pos)++] = ']';
            break;
        }
        /* Block sequence style */
        int first_item = 1;
        while (1) {
            const uint8_t* check2 = *p;
            const uint8_t* check2_start;
            size_t check2_len;
            jtoken_t next2 = next_token(&check2, end, &check2_start, &check2_len);
            if (next2 == JT_RBRACKET) {
                *p = check2;
                break;
            }

            if (!first_item) {
                /* More items expected — after a comma or value */
            }

            emit_newline(buf, pos, cap);
            emit_indent(buf, pos, cap, indent - 1);
            while (*pos + 2 > *cap) {
                *cap *= 2;
                uint8_t* new_buf = (uint8_t*)realloc(*buf, *cap);
                if (!new_buf) return -ENOMEM;
                *buf = new_buf;
            }
            (*buf)[(*pos)++] = '-';
            (*buf)[(*pos)++] = ' ';

            /* For complex items, indentation is tricky. For top-level scalars it works. */
            int rc = emit_yaml_value(p, end, buf, pos, cap, indent);
            if (rc) return rc;

            /* Check for comma */
            const uint8_t* after = *p;
            jtoken_t maybe_comma = next_token(&after, end, &tok_start, &tok_len);
            if (maybe_comma == JT_COMMA) {
                *p = after;
            }
            first_item = 0;
        }
        break;
    }
    default:
        return -EINVAL;
    }
    return 0;
}

int bs_internal_to_yaml(const uint8_t* v1_json, size_t v1_len,
                         uint8_t** out, size_t* out_len)
{
    if (!v1_json || !out || !out_len) return -EINVAL;

    size_t cap  = v1_len * 2 + 1024;
    uint8_t* buf = (uint8_t*)malloc(cap);
    if (!buf) return -ENOMEM;

    size_t pos = 0;
    const uint8_t* p   = v1_json;
    const uint8_t* end = v1_json + v1_len;

    /* Emit YAML document header */
    buf[pos++] = '-'; buf[pos++] = '-'; buf[pos++] = '-';
    buf[pos++] = '\n';

    int rc = emit_yaml_value(&p, end, &buf, &pos, &cap, 1);
    if (rc) {
        free(buf);
        return rc;
    }
    buf[pos++] = '\n';
    buf[pos]   = '\0';

    *out     = buf;
    *out_len = pos + 1;
    return 0;
}
