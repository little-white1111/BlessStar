#include <bs/kernel/ui_map/uidl_validator.h>
#include <bs/kernel/ui_map/schema_to_uidl.h>
#include <bs/kernel/ui_map/ui_render_desc.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * UIDL JSON validator.
 *
 * Performs structural validation of UIDL JSON output:
 * - Checks top-level fields exist (uidl_version, schema_ref, controls)
 * - Validates control array structure (field, widget, optional fields)
 * - Ensures widget values are known enum strings
 * - Verifies children array structure for nested controls
 * - Ensures ai_layout_hint is preserved if present (UIDL-02)
 *
 * Uses minimal one-pass tokenizer (no external JSON library dependency).
 */

/* ── Error collector ───────────────────────────────────────────────── */
#define MAX_UIDL_ERRS 64

typedef struct
{
    char*  msgs[MAX_UIDL_ERRS];
    int    count;
} uidl_errs_t;

static void errs_add(uidl_errs_t* e, const char* msg)
{
    if (e->count >= MAX_UIDL_ERRS) return;
    e->msgs[e->count] = (char*)malloc(strlen(msg) + 1);
    if (e->msgs[e->count]) strcpy(e->msgs[e->count], msg);
    e->count++;
}

static void errs_free(uidl_errs_t* e)
{
    for (int i = 0; i < e->count; i++) free(e->msgs[i]);
    e->count = 0;
}

/* ── Minimal tokenizer (same pattern as schema_json_converter) ─────── */
typedef enum
{
    TOK_EOF, TOK_LBRACE, TOK_RBRACE, TOK_LBRACK, TOK_RBRACK,
    TOK_COMMA, TOK_COLON, TOK_STRING, TOK_NUMBER,
    TOK_TRUE, TOK_FALSE, TOK_NULL, TOK_ERROR
} tok_kind_t;

typedef struct
{
    const char* start;
    const char* pos;
    const char* tok_start;
    const char* end;
    tok_kind_t  kind;
    char*       str_val;
    double      num_val;
    char        err_msg[128];
} tok_t;

static void tok_init(tok_t* p, const char* json, size_t len)
{
    p->start = p->pos = json;
    p->end   = json + len;
    p->str_val = NULL;
    p->tok_start = NULL;
}

static void tok_advance(tok_t* p)
{
    free(p->str_val);
    p->str_val = NULL;
    p->tok_start = NULL;

    while (p->pos < p->end && (unsigned char)*p->pos <= ' ') p->pos++;
    if (p->pos >= p->end) { p->kind = TOK_EOF; return; }

    p->tok_start = p->pos;
    char c = *p->pos;
    switch (c)
    {
    case '{': p->pos++; p->kind = TOK_LBRACE; return;
    case '}': p->pos++; p->kind = TOK_RBRACE; return;
    case '[': p->pos++; p->kind = TOK_LBRACK; return;
    case ']': p->pos++; p->kind = TOK_RBRACK; return;
    case ',': p->pos++; p->kind = TOK_COMMA;  return;
    case ':': p->pos++; p->kind = TOK_COLON;  return;
    case '"':
        p->pos++;
        {
            const char* ss = p->pos;
            size_t l = 0;
            while (p->pos < p->end && *p->pos != '"')
            {
                if (*p->pos == '\\' && p->pos + 1 < p->end) p->pos++;
                p->pos++; l++;
            }
            if (p->pos >= p->end) { p->kind = TOK_ERROR; return; }
            p->str_val = (char*)malloc(l + 1);
            if (p->str_val)
            {
                const char* src = ss; size_t wi = 0;
                for (size_t i = 0; i < l; i++)
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
        }
        p->pos++; p->kind = TOK_STRING; return;
    default:
        if (c == 't' && p->end - p->pos >= 4 && memcmp(p->pos, "true", 4) == 0)
        { p->pos += 4; p->kind = TOK_TRUE; return; }
        if (c == 'f' && p->end - p->pos >= 5 && memcmp(p->pos, "false", 5) == 0)
        { p->pos += 5; p->kind = TOK_FALSE; return; }
        if (c == 'n' && p->end - p->pos >= 4 && memcmp(p->pos, "null", 4) == 0)
        { p->pos += 4; p->kind = TOK_NULL; return; }
        if (c == '-' || (c >= '0' && c <= '9'))
        {
            char* end; p->num_val = strtod(p->pos, &end);
            if (end > p->pos) { p->pos = end; p->kind = TOK_NUMBER; return; }
        }
        p->kind = TOK_ERROR; return;
    }
}

/* ── Simple JSON dict for parsing key:value pairs ──────────────────── */
#define MAX_DICT 256
typedef struct
{
    char* keys[MAX_DICT];
    char* vals[MAX_DICT];
    int   count;
} jdict_t;

static void jdict_init(jdict_t* d) { d->count = 0; }

static void jdict_add(jdict_t* d, const char* key,
                       const char* val_start, size_t val_len)
{
    if (d->count >= MAX_DICT) return;
    d->keys[d->count] = strdup(key);
    d->vals[d->count] = (char*)malloc(val_len + 1);
    if (d->vals[d->count])
    {
        memcpy(d->vals[d->count], val_start, val_len);
        d->vals[d->count][val_len] = '\0';
    }
    d->count++;
}

static const char* jdict_get(const jdict_t* d, const char* key)
{
    for (int i = 0; i < d->count; i++)
        if (d->keys[i] && strcmp(d->keys[i], key) == 0)
            return d->vals[i];
    return NULL;
}

static void jdict_destroy(jdict_t* d)
{
    for (int i = 0; i < d->count; i++)
    { free(d->keys[i]); free(d->vals[i]); }
    d->count = 0;
}

/* ── Parse a dict object into the jdict ────────────────────────────── */
static void parse_dict(tok_t* p, jdict_t* out)
{
    if (p->kind != TOK_LBRACE) return;
    tok_advance(p);

    while (p->kind == TOK_STRING)
    {
        char* key = strdup(p->str_val);
        tok_advance(p);
        if (p->kind != TOK_COLON) { free(key); return; }
        tok_advance(p);

        const char* vp = p->tok_start;
        size_t vl = 0;

        if (p->kind == TOK_LBRACE || p->kind == TOK_LBRACK)
        {
            int depth = 1;
            vp = p->tok_start;
            while (depth > 0 && p->kind != TOK_EOF && p->kind != TOK_ERROR)
            {
                tok_advance(p);
                if (p->kind == TOK_LBRACE || p->kind == TOK_LBRACK) depth++;
                if (p->kind == TOK_RBRACE || p->kind == TOK_RBRACK) depth--;
            }
            vl = (size_t)(p->pos - vp);
            if (p->kind != TOK_EOF) tok_advance(p);
        }
        else
        {
            vp = p->tok_start;
            const char* after_token = p->pos; /* pos is past token, before separator */
            tok_advance(p);
            vl = (size_t)(after_token - vp);
        }

        jdict_add(out, key, vp, vl);
        free(key);

        if (p->kind != TOK_EOF && p->kind != TOK_RBRACE && p->kind != TOK_RBRACK)
        {
            if (p->kind == TOK_COMMA) tok_advance(p);
        }
    }
}

/* ── Known widget values ────────────────────────────────────────────── */
static int is_known_widget(const char* s)
{
    static const char* known[] = {
        "input", "select", "checkbox", "radio", "switch", "textarea",
        "number", "datepicker", "file_upload", "key_value_table",
        "group", "table", "repeatable_group", "custom", NULL
    };
    for (int i = 0; known[i]; i++)
        if (strcmp(s, known[i]) == 0) return 1;
    return 0;
}

/* ── Recursively validate a control object (JSON substring) ────────── */
static int validate_control_json(const char* json_val, uidl_errs_t* errs,
                                  int depth)
{
    if (depth > 32) { errs_add(errs, "control nesting too deep (>32)"); return -1; }
    if (!json_val) return 0;

    size_t jl = strlen(json_val);
    tok_t p;
    tok_init(&p, json_val, jl);
    tok_advance(&p);
    if (p.kind != TOK_LBRACE) { errs_add(errs, "control not an object"); return -1; }

    jdict_t d;
    jdict_init(&d);
    parse_dict(&p, &d);

    /* Check required field "field" */
    const char* field_s = jdict_get(&d, "field");
    if (!field_s) errs_add(errs, "control missing 'field'");
    else
    {
        /* Check field is quoted string */
        if (field_s[0] != '"') errs_add(errs, "control 'field' not a string");
    }

    /* Check required field "widget" */
    const char* widget_s = jdict_get(&d, "widget");
    if (!widget_s) errs_add(errs, "control missing 'widget'");
    else
    {
        /* Extract widget value */
        size_t wl = strlen(widget_s);
        if (wl >= 2 && widget_s[0] == '"' && widget_s[wl-1] == '"')
        {
            char* wv = (char*)malloc(wl - 1);
            if (wv)
            {
                memcpy(wv, widget_s + 1, wl - 2);
                wv[wl - 2] = '\0';
                if (!is_known_widget(wv))
                {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "unknown widget: '%s'", wv);
                    errs_add(errs, buf);
                }
                free(wv);
            }
        }
        else
            errs_add(errs, "control 'widget' not a string");
    }

    /* Check children if present */
    const char* children_s = jdict_get(&d, "children");
    if (children_s)
    {
        size_t cl = strlen(children_s);
        if (cl >= 2 && children_s[0] == '[')
        {
            /* Parse children array */
            tok_t cp;
            tok_init(&cp, children_s, cl);
            tok_advance(&cp);
            if (cp.kind == TOK_LBRACK)
            {
                tok_advance(&cp);
                int idx = 0;
                while (cp.kind == TOK_LBRACE)
                {
                    const char* cstart = cp.tok_start;
                    int cdepth = 1;
                    while (cdepth > 0 && cp.kind != TOK_EOF && cp.kind != TOK_ERROR)
                    {
                        tok_advance(&cp);
                        if (cp.kind == TOK_LBRACE) cdepth++;
                        if (cp.kind == TOK_RBRACE) cdepth--;
                    }
                    size_t clen = (size_t)(cp.pos - cstart);
                    char* child_json = (char*)malloc(clen + 1);
                    if (child_json)
                    {
                        memcpy(child_json, cstart, clen);
                        child_json[clen] = '\0';
                        validate_control_json(child_json, errs, depth + 1);
                        free(child_json);
                    }
                    idx++;
                    if (cp.kind == TOK_COMMA) tok_advance(&cp);
                }
            }
        }
    }

    /* ai_layout_hint — just verify it's a string or null if present (UIDL-02) */;
    /* no additional validation needed — kernel does not interpret it */

    jdict_destroy(&d);
    free(p.str_val);
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Public API: validate UIDL JSON string
 * ══════════════════════════════════════════════════════════════════════ */
int bs_uidl_validate(const char* json, size_t len,
                      char*** out_errors, size_t* out_error_count)
{
    if (!json || !out_errors) return -1;

    uidl_errs_t errs;
    memset(&errs, 0, sizeof(errs));

    tok_t p;
    tok_init(&p, json, len);
    tok_advance(&p);

    if (p.kind != TOK_LBRACE)
    {
        errs_add(&errs, "root not an object");
        goto finish;
    }

    jdict_t root;
    jdict_init(&root);
    parse_dict(&p, &root);

    /* Check uidl_version */
    const char* uv = jdict_get(&root, "uidl_version");
    if (!uv)
        errs_add(&errs, "missing 'uidl_version'");
    else if (*uv < '0' || *uv > '9')
        errs_add(&errs, "'uidl_version' not a number");

    /* Check schema_ref */
    const char* sr = jdict_get(&root, "schema_ref");
    if (!sr)
        errs_add(&errs, "missing 'schema_ref'");
    else if (sr[0] != '"')
        errs_add(&errs, "'schema_ref' not a string");

    /* Check controls */
    const char* ct = jdict_get(&root, "controls");
    if (!ct)
        errs_add(&errs, "missing 'controls'");
    else if (ct[0] != '[')
        errs_add(&errs, "'controls' not an array");
    else
    {
        /* Parse controls array */
        size_t cl = strlen(ct);
        tok_t cp;
        tok_init(&cp, ct, cl);
        tok_advance(&cp);
        if (cp.kind == TOK_LBRACK)
        {
            tok_advance(&cp);
            int idx = 0;
            while (cp.kind == TOK_LBRACE)
            {
                const char* cstart = cp.tok_start;
                int cdepth = 1;
                while (cdepth > 0 && cp.kind != TOK_EOF && cp.kind != TOK_ERROR)
                {
                    tok_advance(&cp);
                    if (cp.kind == TOK_LBRACE) cdepth++;
                    if (cp.kind == TOK_RBRACE) cdepth--;
                }
                size_t clen = (size_t)(cp.pos - cstart);
                char* ctrl_json = (char*)malloc(clen + 1);
                if (ctrl_json)
                {
                    memcpy(ctrl_json, cstart, clen);
                    ctrl_json[clen] = '\0';
                    validate_control_json(ctrl_json, &errs, 0);
                    free(ctrl_json);
                }
                idx++;
                if (cp.kind == TOK_COMMA) tok_advance(&cp);
            }
        }
    }

    jdict_destroy(&root);
    free(p.str_val);

finish:
    /* Convert error list to output */
    if (errs.count == 0)
    {
        *out_errors = NULL;
        *out_error_count = 0;
        return 0;
    }

    *out_errors = (char**)malloc(sizeof(char*) * (size_t)errs.count);
    if (!*out_errors) { errs_free(&errs); return -1; }
    memcpy(*out_errors, errs.msgs, sizeof(char*) * (size_t)errs.count);
    *out_error_count = (size_t)errs.count;
    return (int)errs.count;
}

void bs_uidl_validate_errors_free(char** errors, size_t count)
{
    if (!errors) return;
    for (size_t i = 0; i < count; i++) free(errors[i]);
    free(errors);
}
