/* Gate chain JSON serialization + map + upsert. Zero external dependency. */
#include <bs/kernel/gate_chain/gate_chain_serialize.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Dynamic JSON string builder (inline, mirrors schema_json_converter.c) ── */
typedef struct {
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
    b->buf = new_buf;
    b->cap = new_cap;
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

static void json_buf_destroy(json_buf_t* b) { free(b->buf); b->buf = NULL; }

static void json_escape(const char* s, json_buf_t* b)
{
    json_buf_append(b, "\"");
    while (*s) {
        char c = *s;
        switch (c) {
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

/* ── Minimal JSON tokenizer (mirrors schema_json_converter.c) ── */
typedef enum {
    GTOK_EOF, GTOK_LBRACE, GTOK_RBRACE, GTOK_LBRACK, GTOK_RBRACK,
    GTOK_COMMA, GTOK_COLON, GTOK_STRING, GTOK_NUMBER,
    GTOK_TRUE, GTOK_FALSE, GTOK_NULL, GTOK_ERROR
} gtok_kind_t;

typedef struct {
    const char* pos;
    const char* tok_start;
    const char* end;
    gtok_kind_t kind;
    char*       str_val;
    double      num_val;
} gparse_t;

static void gparse_init(gparse_t* p, const char* json, size_t len)
{
    p->pos = json;
    p->end = json + len;
    p->str_val = NULL;
    p->tok_start = NULL;
}

static void gparse_advance(gparse_t* p)
{
    free(p->str_val);
    p->str_val = NULL;
    p->tok_start = NULL;

    while (p->pos < p->end && (unsigned char)*p->pos <= ' ')
        p->pos++;
    if (p->pos >= p->end) { p->kind = GTOK_EOF; return; }

    p->tok_start = p->pos;
    char c = *p->pos;
    switch (c) {
    case '{': p->pos++; p->kind = GTOK_LBRACE; return;
    case '}': p->pos++; p->kind = GTOK_RBRACE; return;
    case '[': p->pos++; p->kind = GTOK_LBRACK; return;
    case ']': p->pos++; p->kind = GTOK_RBRACK; return;
    case ',': p->pos++; p->kind = GTOK_COMMA;  return;
    case ':': p->pos++; p->kind = GTOK_COLON;  return;
    case '"':
    {
        p->pos++;
        const char* sstart = p->pos;
        size_t len = 0;
        while (p->pos < p->end && *p->pos != '"') {
            if (*p->pos == '\\' && p->pos + 1 < p->end) p->pos++;
            p->pos++; len++;
        }
        if (p->pos >= p->end) { p->kind = GTOK_ERROR; return; }
        p->str_val = (char*)malloc(len + 1);
        if (p->str_val) {
            const char* src = sstart; size_t wi = 0;
            for (size_t i = 0; i < len; i++) {
                if (*src == '\\' && *(src+1)) {
                    src++;
                    switch (*src) {
                    case 'n': p->str_val[wi++] = '\n'; break;
                    case 't': p->str_val[wi++] = '\t'; break;
                    case 'r': p->str_val[wi++] = '\r'; break;
                    case '\\': p->str_val[wi++] = '\\'; break;
                    case '"':  p->str_val[wi++] = '"'; break;
                    default:  p->str_val[wi++] = *src; break;
                    }
                } else p->str_val[wi++] = *src;
                src++;
            }
            p->str_val[wi] = '\0';
        }
        p->pos++; p->kind = GTOK_STRING; return;
    }
    default:
        if (c == 't' && p->end - p->pos >= 4 && memcmp(p->pos, "true", 4) == 0)
        { p->pos += 4; p->kind = GTOK_TRUE; return; }
        if (c == 'f' && p->end - p->pos >= 5 && memcmp(p->pos, "false", 5) == 0)
        { p->pos += 5; p->kind = GTOK_FALSE; return; }
        if (c == 'n' && p->end - p->pos >= 4 && memcmp(p->pos, "null", 4) == 0)
        { p->pos += 4; p->kind = GTOK_NULL; return; }
        if (c == '-' || (c >= '0' && c <= '9')) {
            char* end; p->num_val = strtod(p->pos, &end);
            if (end > p->pos) { p->pos = end; p->kind = GTOK_NUMBER; return; }
        }
        p->kind = GTOK_ERROR; return;
    }
}

/* ── JSON dict (generic key-value pairs) ── */
#define GMAX_DICT 256

typedef struct {
    const char* keys[GMAX_DICT];
    char*       vals[GMAX_DICT]; /* owned JSON substring */
    int         count;
} gjson_dict_t;

static void gjson_dict_init(gjson_dict_t* d) { d->count = 0; }

static void gjson_dict_add(gjson_dict_t* d, const char* key,
                            const char* val_str /* already unquoted */)
{
    if (d->count >= GMAX_DICT) return;
    d->keys[d->count] = strdup(key);
    d->vals[d->count] = val_str ? strdup(val_str) : NULL;
    d->count++;
}

static const char* gjson_dict_get(const gjson_dict_t* d, const char* key)
{
    for (int i = 0; i < d->count; i++)
        if (d->keys[i] && strcmp(d->keys[i], key) == 0)
            return d->vals[i];
    return NULL;
}

static void gjson_dict_destroy(gjson_dict_t* d)
{
    for (int i = 0; i < d->count; i++) {
        free((void*)d->keys[i]);
        free(d->vals[i]);
    }
    d->count = 0;
}

/* Parse dict object into key/value pairs. Values are stored unquoted. */
static void gparse_dict_into(gparse_t* p, gjson_dict_t* out)
{
    if (p->kind != GTOK_LBRACE) return;
    gparse_advance(p);

    while (p->kind == GTOK_STRING) {
        char* key = strdup(p->str_val);
        gparse_advance(p);
        if (p->kind != GTOK_COLON) { free(key); return; }
        gparse_advance(p);

        /* Capture value: use str_val for strings, raw text for composites */
        if (p->kind == GTOK_STRING) {
            gjson_dict_add(out, key, p->str_val);
            gparse_advance(p);
        } else if (p->kind == GTOK_NUMBER) {
            char num_buf[64];
            snprintf(num_buf, sizeof(num_buf), "%g", p->num_val);
            gjson_dict_add(out, key, num_buf);
            gparse_advance(p);
        } else if (p->kind == GTOK_TRUE || p->kind == GTOK_FALSE || p->kind == GTOK_NULL) {
            const char* lit = (p->kind == GTOK_TRUE ? "true" : (p->kind == GTOK_FALSE ? "false" : "null"));
            gjson_dict_add(out, key, lit);
            gparse_advance(p);
        } else if (p->kind == GTOK_LBRACE || p->kind == GTOK_LBRACK) {
            /* Copy raw JSON for composites */
            const char* val_pos = p->tok_start;
            int depth = 1;
            while (depth > 0 && p->kind != GTOK_EOF && p->kind != GTOK_ERROR) {
                gparse_advance(p);
                if (p->kind == GTOK_LBRACE || p->kind == GTOK_LBRACK) depth++;
                if (p->kind == GTOK_RBRACE || p->kind == GTOK_RBRACK) depth--;
            }
            size_t val_len = (size_t)(p->pos - val_pos);
            if (p->kind != GTOK_EOF) gparse_advance(p);
            char* raw = (char*)malloc(val_len + 1);
            if (raw) { memcpy(raw, val_pos, val_len); raw[val_len] = '\0'; }
            gjson_dict_add(out, key, raw);
            free(raw);
        } else {
            gjson_dict_add(out, key, "");
            gparse_advance(p);
        }

        free(key);

        if (p->kind == GTOK_COMMA) gparse_advance(p);
    }

    /* Advance past closing delimiter */
    if (p->kind == GTOK_RBRACE || p->kind == GTOK_RBRACK)
        gparse_advance(p);
}

/* Parse JSON string array into char** */
static char** parse_string_array(gparse_t* p, size_t* out_count)
{
    if (p->kind != GTOK_LBRACK) return NULL;
    gparse_advance(p);

    size_t cap = 8, cnt = 0;
    char** arr = (char**)calloc(cap, sizeof(char*));
    if (!arr) return NULL;

    while (p->kind == GTOK_STRING) {
        if (cnt >= cap) {
            cap *= 2;
            char** new_arr = (char**)realloc(arr, cap * sizeof(char*));
            if (!new_arr) { free(arr); return NULL; }
            arr = new_arr;
        }
        arr[cnt++] = strdup(p->str_val);
        gparse_advance(p);
        if (p->kind == GTOK_COMMA) gparse_advance(p);
    }

    if (p->kind != GTOK_RBRACK) { /* error */ }
    if (p->kind != GTOK_EOF) gparse_advance(p);

    *out_count = cnt;
    return arr;
}

/* Extract quoted string (strip outer "") */
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

/* ══════════════════════════════════════════════════════════════════════
 * bs_gate_chain_free
 * ══════════════════════════════════════════════════════════════════════ */
void bs_gate_chain_free(bs_gate_chain_t* chain)
{
    if (!chain) return;
    free(chain->version);
    for (size_t i = 0; i < chain->node_count; i++) {
        bs_gate_node_t* n = &chain->nodes[i];
        free(n->type);
        free(n->id);
        free(n->field_key);
        free(n->op);
        free(n->value);
        /* ── OPT-08: free new semantic fields ── */
        free(n->stable_key);
        free(n->sub_category);
        free(n->domain);
        free(n->entity);
        if (n->child_ids) {
            for (size_t j = 0; j < n->child_count; j++)
                free(n->child_ids[j]);
            free(n->child_ids);
        }
        if (n->do_ids) {
            for (size_t j = 0; j < n->do_count; j++)
                free(n->do_ids[j]);
            free(n->do_ids);
        }
    }
    free(chain->nodes);
    bs_gate_map_free(chain->map);
    free(chain);
}

/* ══════════════════════════════════════════════════════════════════════
 * bs_gate_chain_to_json
 * ══════════════════════════════════════════════════════════════════════ */
int bs_gate_chain_to_json(const bs_gate_chain_t* chain,
                            char** out_json, size_t* out_len)
{
    if (!chain || !out_json) return -1;

    json_buf_t b;
    if (json_buf_init(&b)) return -1;

    json_buf_append(&b, "{\n");
    json_buf_appendf(&b, "\"version\":%s,\n", chain->version ? chain->version : "\"1.0\"");
    json_buf_append(&b, "\"gates\":[\n");

    for (size_t i = 0; i < chain->node_count; i++) {
        if (i > 0) json_buf_append(&b, ",\n");
        const bs_gate_node_t* n = &chain->nodes[i];
        json_buf_append(&b, "  {");
        int first = 1;

        if (n->type) {
            if (!first) json_buf_append(&b, ",");
            json_buf_append(&b, "\"type\":");
            json_escape(n->type, &b);
            first = 0;
        }
        if (n->id) {
            if (!first) json_buf_append(&b, ",");
            json_buf_append(&b, "\"id\":");
            json_escape(n->id, &b);
            first = 0;
        }
        if (n->field_key) {
            if (!first) json_buf_append(&b, ",");
            json_buf_append(&b, "\"field_key\":");
            json_escape(n->field_key, &b);
            first = 0;
        }
        if (n->op) {
            if (!first) json_buf_append(&b, ",");
            json_buf_append(&b, "\"op\":");
            json_escape(n->op, &b);
            first = 0;
        }
        if (n->value) {
            if (!first) json_buf_append(&b, ",");
            json_buf_append(&b, "\"value\":");
            json_escape(n->value, &b);
            first = 0;
        }
        /* ── OPT-08: serialize layer, stable_key, sub_category, domain, entity ── */
        if (n->layer >= 0 && n->layer < BS_GATE_LAYER_COUNT) {
            if (!first) json_buf_append(&b, ",");
            json_buf_appendf(&b, "\"layer\":%d", n->layer);
            first = 0;
        }
        if (n->stable_key) {
            if (!first) json_buf_append(&b, ",");
            json_buf_append(&b, "\"stable_key\":");
            json_escape(n->stable_key, &b);
            first = 0;
        }
        if (n->sub_category) {
            if (!first) json_buf_append(&b, ",");
            json_buf_append(&b, "\"sub_category\":");
            json_escape(n->sub_category, &b);
            first = 0;
        }
        if (n->domain) {
            if (!first) json_buf_append(&b, ",");
            json_buf_append(&b, "\"domain\":");
            json_escape(n->domain, &b);
            first = 0;
        }
        if (n->entity) {
            if (!first) json_buf_append(&b, ",");
            json_buf_append(&b, "\"entity\":");
            json_escape(n->entity, &b);
            first = 0;
        }
        /* child_ids */
        if (n->child_count > 0 && n->child_ids) {
            if (!first) json_buf_append(&b, ",");
            json_buf_append(&b, "\"children\":[");
            for (size_t j = 0; j < n->child_count; j++) {
                if (j > 0) json_buf_append(&b, ",");
                json_escape(n->child_ids[j], &b);
            }
            json_buf_append(&b, "]");
            first = 0;
        }
        /* do_ids */
        if (n->do_count > 0 && n->do_ids) {
            if (!first) json_buf_append(&b, ",");
            json_buf_append(&b, "\"do\":[");
            for (size_t j = 0; j < n->do_count; j++) {
                if (j > 0) json_buf_append(&b, ",");
                json_escape(n->do_ids[j], &b);
            }
            json_buf_append(&b, "]");
            first = 0;
        }
        json_buf_append(&b, "}");
    }

    json_buf_append(&b, "\n]}\n");

    *out_json = b.buf;
    if (out_len) *out_len = b.len;
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * bs_gate_chain_from_json — direct token-stream parser (no dict capture)
 * ══════════════════════════════════════════════════════════════════════ */
int bs_gate_chain_from_json(const char* json, bs_gate_chain_t** out)
{
    if (!json || !out) return -1;

    size_t len = strlen(json);
    gparse_t p;
    gparse_init(&p, json, len);

    bs_gate_chain_t* chain = (bs_gate_chain_t*)calloc(1, sizeof(bs_gate_chain_t));
    if (!chain) return -1;
    chain->version = strdup("1.0");

    /* Parse top-level { ... } */
    gparse_advance(&p);
    if (p.kind != GTOK_LBRACE) { bs_gate_chain_free(chain); return -1; }
    gparse_advance(&p);

    while (p.kind == GTOK_STRING) {
        char* key = strdup(p.str_val);
        gparse_advance(&p); /* skip string */
        if (p.kind != GTOK_COLON) { free(key); bs_gate_chain_free(chain); return -1; }
        gparse_advance(&p); /* skip colon */

        if (strcmp(key, "version") == 0) {
            if (p.kind == GTOK_STRING) {
                free(chain->version);
                chain->version = strdup(p.str_val);
            }
            gparse_advance(&p);
        } else if (strcmp(key, "gates") == 0) {
            if (p.kind == GTOK_LBRACK) {
                gparse_advance(&p); /* skip [ */

                size_t cap = 16, cnt = 0;
                chain->nodes = (bs_gate_node_t*)calloc(cap, sizeof(bs_gate_node_t));

                while (p.kind == GTOK_LBRACE) {
                    if (cnt >= cap) {
                        cap *= 2;
                        bs_gate_node_t* new_nodes = (bs_gate_node_t*)
                            realloc(chain->nodes, cap * sizeof(bs_gate_node_t));
                        if (!new_nodes) break;
                        chain->nodes = new_nodes;
                    }

                    bs_gate_node_t* n = &chain->nodes[cnt];
                    memset(n, 0, sizeof(*n));
                    gparse_advance(&p); /* skip { */

                    while (p.kind == GTOK_STRING) {
                        char* fk = strdup(p.str_val);
                        gparse_advance(&p);
                        if (p.kind != GTOK_COLON) { free(fk); break; }
                        gparse_advance(&p);

                        if (strcmp(fk, "type") == 0 && p.kind == GTOK_STRING) {
                            n->type = strdup(p.str_val);
                            gparse_advance(&p);
                        } else if (strcmp(fk, "id") == 0 && p.kind == GTOK_STRING) {
                            n->id = strdup(p.str_val);
                            gparse_advance(&p);
                        } else if (strcmp(fk, "field_key") == 0 && p.kind == GTOK_STRING) {
                            n->field_key = strdup(p.str_val);
                            gparse_advance(&p);
                        } else if (strcmp(fk, "op") == 0 && p.kind == GTOK_STRING) {
                            n->op = strdup(p.str_val);
                            gparse_advance(&p);
                        } else if (strcmp(fk, "value") == 0 && p.kind == GTOK_STRING) {
                            n->value = strdup(p.str_val);
                            gparse_advance(&p);
                        } else if (strcmp(fk, "children") == 0 && p.kind == GTOK_LBRACK)
                            n->child_ids = parse_string_array(&p, &n->child_count);
                        else if (strcmp(fk, "do") == 0 && p.kind == GTOK_LBRACK)
                            n->do_ids = parse_string_array(&p, &n->do_count);
                        /* ── OPT-08: parse new semantic fields ── */
                        else if (strcmp(fk, "layer") == 0 && p.kind == GTOK_NUMBER) {
                            n->layer = (int)p.num_val;
                            gparse_advance(&p);
                        } else if (strcmp(fk, "stable_key") == 0 && p.kind == GTOK_STRING) {
                            n->stable_key = strdup(p.str_val);
                            gparse_advance(&p);
                        } else if (strcmp(fk, "sub_category") == 0 && p.kind == GTOK_STRING) {
                            n->sub_category = strdup(p.str_val);
                            gparse_advance(&p);
                        } else if (strcmp(fk, "domain") == 0 && p.kind == GTOK_STRING) {
                            n->domain = strdup(p.str_val);
                            gparse_advance(&p);
                        } else if (strcmp(fk, "entity") == 0 && p.kind == GTOK_STRING) {
                            n->entity = strdup(p.str_val);
                            gparse_advance(&p);
                        }
                        else
                            gparse_advance(&p); /* skip unknown value */

                        free(fk);
                        if (p.kind == GTOK_COMMA) gparse_advance(&p);
                    }

                    /* Advance past closing } */
                    if (p.kind == GTOK_RBRACE) gparse_advance(&p);
                    cnt++;
                    if (p.kind == GTOK_COMMA) gparse_advance(&p);
                }
                chain->node_count = cnt;
            } else {
                gparse_advance(&p);
            }
        } else {
            /* Skip unknown key's value */
            if (p.kind == GTOK_LBRACE || p.kind == GTOK_LBRACK) {
                int depth = 1;
                while (depth > 0 && p.kind != GTOK_EOF && p.kind != GTOK_ERROR) {
                    gparse_advance(&p);
                    if (p.kind == GTOK_LBRACE || p.kind == GTOK_LBRACK) depth++;
                    if (p.kind == GTOK_RBRACE || p.kind == GTOK_RBRACK) depth--;
                }
            } else {
                gparse_advance(&p);
            }
        }

        free(key);
        if (p.kind == GTOK_COMMA) gparse_advance(&p);
    }

    free(p.str_val);
    *out = chain;

    /* ── OPT-08: Rebuild map after deserializing all nodes ── */
    if (chain->node_count > 0) {
        bs_gate_map_create(&chain->map, chain->node_count * 2);
        for (size_t i = 0; i < chain->node_count; i++) {
            if (chain->nodes[i].stable_key) {
                bs_gate_map_insert(chain->map, chain->nodes[i].stable_key, i);
            }
        }
    }

    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * bs_gate_map — FNV-1a hash + open addressing
 * ══════════════════════════════════════════════════════════════════════ */

/* Compute next power of 2 >= n */
static size_t next_pow2(size_t n)
{
    if (n == 0) return 1;
    size_t v = 1;
    while (v < n) v <<= 1;
    return v;
}

/* FNV-1a 64-bit hash of a string */
static uint64_t fnv1a_hash(const char* s)
{
    uint64_t hash = 14695981039346656037ULL;
    while (*s) {
        hash ^= (uint64_t)(unsigned char)*s;
        hash *= 1099511628211ULL;
        s++;
    }
    return hash;
}

/* SLOT_EMPTY sentinel: empty slot (slot->stable_key == NULL) */
/* SLOT_TOMB: tombstone marker (use a static const sentinel ptr) */
static const char kMapTombstone[1] = {0};

#define SLOT_EMPTY(s)   ((s).stable_key == NULL)
#define SLOT_TOMB(s)    ((s).stable_key == kMapTombstone)

static bs_gate_map_slot_t* map_find_slot(bs_gate_map_t* map,
                                          const char* stable_key,
                                          uint64_t hash)
{
    size_t idx = (size_t)(hash & (map->capacity - 1));
    bs_gate_map_slot_t* tombstone = NULL;

    for (size_t i = 0; i < map->capacity; i++) {
        bs_gate_map_slot_t* slot = &map->slots[idx];
        if (SLOT_EMPTY(*slot)) {
            return tombstone ? tombstone : slot;
        }
        if (SLOT_TOMB(*slot)) {
            if (!tombstone) tombstone = slot;
            idx = (idx + 1) & (map->capacity - 1);
            continue;
        }
        if (strcmp(slot->stable_key, stable_key) == 0) {
            return slot;
        }
        idx = (idx + 1) & (map->capacity - 1);
    }
    return tombstone;
}

int bs_gate_map_create(bs_gate_map_t** out, size_t capacity)
{
    if (!out) return -1;
    size_t cap = next_pow2(capacity);
    if (cap < 8) cap = 8;

    bs_gate_map_t* map = (bs_gate_map_t*)calloc(1, sizeof(bs_gate_map_t));
    if (!map) return -1;
    map->slots = (bs_gate_map_slot_t*)calloc(cap, sizeof(bs_gate_map_slot_t));
    if (!map->slots) { free(map); return -1; }
    map->capacity = cap;
    map->count = 0;
    *out = map;
    return 0;
}

void bs_gate_map_free(bs_gate_map_t* map)
{
    if (!map) return;
    if (map->slots) {
        for (size_t i = 0; i < map->capacity; i++) {
            if (map->slots[i].stable_key &&
                map->slots[i].stable_key != kMapTombstone) {
                free(map->slots[i].stable_key);
            }
        }
        free(map->slots);
    }
    free(map);
}

int bs_gate_map_rebuild(bs_gate_map_t* map)
{
    if (!map || !map->slots) return -1;

    size_t new_cap = next_pow2(map->count * 3 / 2);
    if (new_cap < 8) new_cap = 8;
    if (new_cap == map->capacity) return 0;

    bs_gate_map_slot_t* new_slots =
        (bs_gate_map_slot_t*)calloc(new_cap, sizeof(bs_gate_map_slot_t));
    if (!new_slots) return -1;

    bs_gate_map_slot_t* old_slots = map->slots;
    size_t old_cap = map->capacity;

    map->slots = new_slots;
    map->capacity = new_cap;
    map->count = 0;

    for (size_t i = 0; i < old_cap; i++) {
        if (old_slots[i].stable_key &&
            old_slots[i].stable_key != kMapTombstone) {
            bs_gate_map_insert(map, old_slots[i].stable_key,
                               old_slots[i].node_index);
            free(old_slots[i].stable_key);
        }
    }
    free(old_slots);
    return 0;
}

int bs_gate_map_insert(bs_gate_map_t* map, const char* stable_key,
                        size_t node_index)
{
    if (!map || !stable_key) return -1;

    /* Rebuild if load factor > 0.7 */
    if (map->count + 1 > map->capacity * 7 / 10) {
        if (bs_gate_map_rebuild(map) != 0) return -1;
    }

    uint64_t hash = fnv1a_hash(stable_key);
    bs_gate_map_slot_t* slot = map_find_slot(map, stable_key, hash);
    if (!slot) return -1;

    if (!SLOT_EMPTY(*slot) && !SLOT_TOMB(*slot)) {
        /* Overwrite existing: free old key */
        free(slot->stable_key);
    } else {
        map->count++;
    }
    slot->stable_key = strdup(stable_key);
    slot->node_index = node_index;
    return 0;
}

int bs_gate_map_lookup(const bs_gate_map_t* map, const char* stable_key,
                        size_t* out_index)
{
    if (!map || !stable_key || !out_index) return -1;
    if (map->count == 0) return -1;

    uint64_t hash = fnv1a_hash(stable_key);
    size_t idx = (size_t)(hash & (map->capacity - 1));

    for (size_t i = 0; i < map->capacity; i++) {
        const bs_gate_map_slot_t* slot = &map->slots[idx];
        if (SLOT_EMPTY(*slot)) return -1;
        if (!SLOT_TOMB(*slot) && strcmp(slot->stable_key, stable_key) == 0) {
            *out_index = slot->node_index;
            return 0;
        }
        idx = (idx + 1) & (map->capacity - 1);
    }
    return -1;
}

/* ══════════════════════════════════════════════════════════════════════
 * bs_gate_chain_upsert — idempotent write via stable_key
 * ══════════════════════════════════════════════════════════════════════ */
int bs_gate_chain_upsert(bs_gate_chain_t* chain, const bs_gate_node_t* node,
                          size_t* out_index)
{
    if (!chain || !node) return -1;

    /* Lazy-init map */
    if (!chain->map && chain->node_count > 0) {
        bs_gate_map_create(&chain->map, chain->node_count * 2);
        for (size_t i = 0; i < chain->node_count; i++) {
            if (chain->nodes[i].stable_key) {
                bs_gate_map_insert(chain->map, chain->nodes[i].stable_key, i);
            }
        }
    }

    /* Try lookup by stable_key */
    if (node->stable_key && chain->map) {
        size_t existing_idx;
        if (bs_gate_map_lookup(chain->map, node->stable_key, &existing_idx) == 0) {
            /* Overwrite existing node at existing_idx */
            bs_gate_node_t* dst = &chain->nodes[existing_idx];
            /* Free old fields before overwrite */
            free(dst->type); free(dst->id); free(dst->field_key);
            free(dst->op); free(dst->value);
            free(dst->stable_key); free(dst->sub_category);
            free(dst->domain); free(dst->entity);
            if (dst->child_ids) {
                for (size_t j = 0; j < dst->child_count; j++) free(dst->child_ids[j]);
                free(dst->child_ids);
            }
            if (dst->do_ids) {
                for (size_t j = 0; j < dst->do_count; j++) free(dst->do_ids[j]);
                free(dst->do_ids);
            }
            /* Copy new fields */
            memset(dst, 0, sizeof(*dst));
            dst->type = node->type ? strdup(node->type) : NULL;
            dst->id = node->id ? strdup(node->id) : NULL;
            dst->field_key = node->field_key ? strdup(node->field_key) : NULL;
            dst->op = node->op ? strdup(node->op) : NULL;
            dst->value = node->value ? strdup(node->value) : NULL;
            dst->stable_key = node->stable_key ? strdup(node->stable_key) : NULL;
            dst->layer = node->layer;
            dst->sub_category = node->sub_category ? strdup(node->sub_category) : NULL;
            dst->domain = node->domain ? strdup(node->domain) : NULL;
            dst->entity = node->entity ? strdup(node->entity) : NULL;
            if (node->child_count > 0 && node->child_ids) {
                dst->child_count = node->child_count;
                dst->child_ids = (char**)malloc(node->child_count * sizeof(char*));
                for (size_t j = 0; j < node->child_count; j++)
                    dst->child_ids[j] = strdup(node->child_ids[j]);
            }
            if (node->do_count > 0 && node->do_ids) {
                dst->do_count = node->do_count;
                dst->do_ids = (char**)malloc(node->do_count * sizeof(char*));
                for (size_t j = 0; j < node->do_count; j++)
                    dst->do_ids[j] = strdup(node->do_ids[j]);
            }
            if (out_index) *out_index = existing_idx;
            return 0;
        }
    }

    /* Append new node at end */
    size_t new_count = chain->node_count + 1;
    bs_gate_node_t* new_nodes = (bs_gate_node_t*)realloc(
        chain->nodes, new_count * sizeof(bs_gate_node_t));
    if (!new_nodes) return -1;
    chain->nodes = new_nodes;

    bs_gate_node_t* dst = &chain->nodes[chain->node_count];
    memset(dst, 0, sizeof(*dst));
    dst->type = node->type ? strdup(node->type) : NULL;
    dst->id = node->id ? strdup(node->id) : NULL;
    dst->field_key = node->field_key ? strdup(node->field_key) : NULL;
    dst->op = node->op ? strdup(node->op) : NULL;
    dst->value = node->value ? strdup(node->value) : NULL;
    dst->stable_key = node->stable_key ? strdup(node->stable_key) : NULL;
    dst->layer = node->layer;
    dst->sub_category = node->sub_category ? strdup(node->sub_category) : NULL;
    dst->domain = node->domain ? strdup(node->domain) : NULL;
    dst->entity = node->entity ? strdup(node->entity) : NULL;
    if (node->child_count > 0 && node->child_ids) {
        dst->child_count = node->child_count;
        dst->child_ids = (char**)malloc(node->child_count * sizeof(char*));
        for (size_t j = 0; j < node->child_count; j++)
            dst->child_ids[j] = strdup(node->child_ids[j]);
    }
    if (node->do_count > 0 && node->do_ids) {
        dst->do_count = node->do_count;
        dst->do_ids = (char**)malloc(node->do_count * sizeof(char*));
        for (size_t j = 0; j < node->do_count; j++)
            dst->do_ids[j] = strdup(node->do_ids[j]);
    }

    size_t new_idx = chain->node_count;
    chain->node_count = new_count;

    /* Insert into map */
    if (dst->stable_key) {
        if (!chain->map) bs_gate_map_create(&chain->map, 16);
        if (chain->map) bs_gate_map_insert(chain->map, dst->stable_key, new_idx);
    }

    if (out_index) *out_index = new_idx;
    return 0;
}
