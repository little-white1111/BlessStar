/* Gate chain JSON serialization + map + upsert. Zero external dependency.
 * DAG version: pointer-based tree, recursive DFS traversal. */
#include <bs/kernel/gate_chain/gate_chain_serialize.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Dynamic JSON string builder ────────────────────────────────────── */
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

/* ══════════════════════════════════════════════════════════════════════
 * Lifecycle
 * ══════════════════════════════════════════════════════════════════════ */

bs_gate_chain_t* bs_gate_chain_create(void)
{
    bs_gate_chain_t* c = (bs_gate_chain_t*)calloc(1, sizeof(bs_gate_chain_t));
    if (c) c->version = strdup("1.0");
    return c;
}

bs_gate_node_t* bs_gate_node_create(const char* type, const char* id)
{
    bs_gate_node_t* n = (bs_gate_node_t*)calloc(1, sizeof(bs_gate_node_t));
    if (!n) return NULL;
    if (type) n->type = strdup(type);
    if (id)   n->id = strdup(id);
    return n;
}

void bs_gate_node_free(bs_gate_node_t* node)
{
    if (!node) return;
    free(node->type);
    free(node->id);
    free(node->field_key);
    free(node->op);
    free(node->value);
    free(node->stable_key);
    free(node->sub_category);
    free(node->domain);
    free(node->entity);

    /* Recursively free children */
    for (size_t i = 0; i < node->child_count; i++)
        bs_gate_node_free(node->children[i]);
    free(node->children);

    /* Recursively free do_nodes */
    for (size_t i = 0; i < node->do_count; i++)
        bs_gate_node_free(node->do_nodes[i]);
    free(node->do_nodes);

    free(node);
}

/* ── Node linking ───────────────────────────────────────────────────── */
int bs_gate_node_link_child(bs_gate_node_t* parent, bs_gate_node_t* child)
{
    if (!parent || !child) return -1;
    bs_gate_node_t** new_c = (bs_gate_node_t**)realloc(parent->children,
        (parent->child_count + 1) * sizeof(bs_gate_node_t*));
    if (!new_c) return -1;
    parent->children = new_c;
    parent->children[parent->child_count++] = child;
    return 0;
}

int bs_gate_node_link_do(bs_gate_node_t* parent, bs_gate_node_t* do_node)
{
    if (!parent || !do_node) return -1;
    bs_gate_node_t** new_d = (bs_gate_node_t**)realloc(parent->do_nodes,
        (parent->do_count + 1) * sizeof(bs_gate_node_t*));
    if (!new_d) return -1;
    parent->do_nodes = new_d;
    parent->do_nodes[parent->do_count++] = do_node;
    return 0;
}

/* ── DFS node count ─────────────────────────────────────────────────── */
typedef struct {
    bs_gate_node_t** nodes;
    size_t count;
    size_t cap;
} node_set_t;

static bool node_set_contains(node_set_t* set, bs_gate_node_t* n)
{
    for (size_t i = 0; i < set->count; i++)
        if (set->nodes[i] == n) return true;
    return false;
}

static int node_set_add(node_set_t* set, bs_gate_node_t* n)
{
    if (set->count >= set->cap) {
        set->cap = set->cap ? set->cap * 2 : 64;
        bs_gate_node_t** new_n = (bs_gate_node_t**)realloc(set->nodes, set->cap * sizeof(bs_gate_node_t*));
        if (!new_n) return -1;
        set->nodes = new_n;
    }
    set->nodes[set->count++] = n;
    return 0;
}

static void dfs_collect(bs_gate_node_t* node, node_set_t* set)
{
    if (!node || node_set_contains(set, node)) return;
    node_set_add(set, node);
    for (size_t i = 0; i < node->child_count; i++)
        dfs_collect(node->children[i], set);
    for (size_t i = 0; i < node->do_count; i++)
        dfs_collect(node->do_nodes[i], set);
}

size_t bs_gate_chain_node_count(const bs_gate_chain_t* chain)
{
    if (!chain || !chain->root) return 0;
    node_set_t set;
    memset(&set, 0, sizeof(set));
    dfs_collect(chain->root, &set);
    size_t count = set.count;
    free(set.nodes);
    return count;
}

/* ══════════════════════════════════════════════════════════════════════
 * bs_gate_chain_free
 * ══════════════════════════════════════════════════════════════════════ */
void bs_gate_chain_free(bs_gate_chain_t* chain)
{
    if (!chain) return;
    free(chain->version);
    bs_gate_node_free(chain->root);   /* recursive DFS free */
    bs_gate_map_free(chain->map);
    free(chain);
}

/* ══════════════════════════════════════════════════════════════════════
 * Serialization: node → JSON (recursive)
 * ══════════════════════════════════════════════════════════════════════ */

static int serialize_node(const bs_gate_node_t* n, json_buf_t* b, int indent);

static int serialize_node_array(const bs_gate_node_t** arr, size_t count,
                                 json_buf_t* b, int indent)
{
    json_buf_append(b, "[\n");
    for (size_t i = 0; i < count; i++) {
        if (i > 0) json_buf_append(b, ",\n");
        for (int j = 0; j < indent + 1; j++) json_buf_append(b, "  ");
        serialize_node(arr[i], b, indent + 1);
    }
    json_buf_append(b, "\n");
    for (int j = 0; j < indent; j++) json_buf_append(b, "  ");
    json_buf_append(b, "]");
    return 0;
}

static int serialize_node(const bs_gate_node_t* n, json_buf_t* b, int indent)
{
    if (!n) { json_buf_append(b, "null"); return 0; }

    json_buf_append(b, "{");
    int first = 1;

    /* Scalar fields */
    if (n->type) {
        if (!first) json_buf_append(b, ",");
        json_buf_append(b, "\"type\":");
        json_escape(n->type, b);
        first = 0;
    }
    if (n->id) {
        if (!first) json_buf_append(b, ",");
        json_buf_append(b, "\"id\":");
        json_escape(n->id, b);
        first = 0;
    }
    if (n->field_key) {
        if (!first) json_buf_append(b, ",");
        json_buf_append(b, "\"field_key\":");
        json_escape(n->field_key, b);
        first = 0;
    }
    if (n->op) {
        if (!first) json_buf_append(b, ",");
        json_buf_append(b, "\"op\":");
        json_escape(n->op, b);
        first = 0;
    }
    if (n->value) {
        if (!first) json_buf_append(b, ",");
        json_buf_append(b, "\"value\":");
        json_escape(n->value, b);
        first = 0;
    }

    /* Semantic fields */
    if (n->layer >= 0 && n->layer < BS_GATE_LAYER_COUNT) {
        if (!first) json_buf_append(b, ",");
        json_buf_appendf(b, "\"layer\":%d", n->layer);
        first = 0;
    }
    if (n->stable_key) {
        if (!first) json_buf_append(b, ",");
        json_buf_append(b, "\"stable_key\":");
        json_escape(n->stable_key, b);
        first = 0;
    }
    if (n->sub_category) {
        if (!first) json_buf_append(b, ",");
        json_buf_append(b, "\"sub_category\":");
        json_escape(n->sub_category, b);
        first = 0;
    }
    if (n->domain) {
        if (!first) json_buf_append(b, ",");
        json_buf_append(b, "\"domain\":");
        json_escape(n->domain, b);
        first = 0;
    }
    if (n->entity) {
        if (!first) json_buf_append(b, ",");
        json_buf_append(b, "\"entity\":");
        json_escape(n->entity, b);
        first = 0;
    }

    /* DAG children array (recursive sub-objects) */
    if (n->child_count > 0 && n->children) {
        if (!first) json_buf_append(b, ",");
        json_buf_append(b, "\n");
        for (int j = 0; j < indent + 1; j++) json_buf_append(b, "  ");
        json_buf_append(b, "\"children\":");
        serialize_node_array((const bs_gate_node_t**)n->children, n->child_count, b, indent + 1);
        first = 0;
    }
    /* DAG do_nodes array (recursive sub-objects) */
    if (n->do_count > 0 && n->do_nodes) {
        if (!first) json_buf_append(b, ",");
        json_buf_append(b, "\n");
        for (int j = 0; j < indent + 1; j++) json_buf_append(b, "  ");
        json_buf_append(b, "\"do\":");
        serialize_node_array((const bs_gate_node_t**)n->do_nodes, n->do_count, b, indent + 1);
        first = 0;
    }

    json_buf_append(b, "}");
    return 0;
}

int bs_gate_chain_to_json(const bs_gate_chain_t* chain,
                           char** out_json, size_t* out_len)
{
    if (!chain || !out_json) return -1;

    json_buf_t b;
    if (json_buf_init(&b)) return -1;

    json_buf_append(&b, "{\n");
    json_buf_appendf(&b, "  \"version\":\"%s\",\n",
                     chain->version ? chain->version : "1.0");

    /* Serialize root recursively */
    json_buf_append(&b, "  \"root\":");
    if (chain->root)
        serialize_node(chain->root, &b, 1);
    else
        json_buf_append(&b, "null");

    json_buf_append(&b, "\n}\n");

    *out_json = b.buf;
    if (out_len) *out_len = b.len;
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * JSON tokenizer (minimal, mirrors schema_json_converter.c)
 * ══════════════════════════════════════════════════════════════════════ */
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

static void gparse_free(gparse_t* p)
{
    free(p->str_val);
    p->str_val = NULL;
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

/* ── Recursive JSON → node parser ───────────────────────────────────── */

static bs_gate_node_t* parse_node(gparse_t* p);

/* Parse a JSON array of nodes: [ {...}, {...} ] */
static bs_gate_node_t** parse_node_array(gparse_t* p, size_t* out_count)
{
    if (p->kind != GTOK_LBRACK) return NULL;
    gparse_advance(p);

    size_t cap = 8, cnt = 0;
    bs_gate_node_t** arr = (bs_gate_node_t**)calloc(cap, sizeof(bs_gate_node_t*));
    if (!arr) return NULL;

    while (p->kind == GTOK_LBRACE) {
        if (cnt >= cap) {
            cap *= 2;
            bs_gate_node_t** new_arr = (bs_gate_node_t**)realloc(arr, cap * sizeof(bs_gate_node_t*));
            if (!new_arr) { free(arr); return NULL; }
            arr = new_arr;
        }
        arr[cnt++] = parse_node(p);
        if (p->kind == GTOK_COMMA) gparse_advance(p);
    }

    if (p->kind != GTOK_RBRACK) { /* ignore parse error */ }
    if (p->kind != GTOK_EOF) gparse_advance(p);

    *out_count = cnt;
    return arr;
}

/* Parse a single node object */
static bs_gate_node_t* parse_node(gparse_t* p)
{
    if (p->kind != GTOK_LBRACE) return NULL;

    bs_gate_node_t* n = (bs_gate_node_t*)calloc(1, sizeof(bs_gate_node_t));
    if (!n) return NULL;

    gparse_advance(p); /* skip { */

    while (p->kind == GTOK_STRING) {
        char* key = strdup(p->str_val);
        gparse_advance(p);
        if (p->kind != GTOK_COLON) { free(key); break; }
        gparse_advance(p);

        if (strcmp(key, "type") == 0 && p->kind == GTOK_STRING) {
            n->type = strdup(p->str_val);
            gparse_advance(p);
        } else if (strcmp(key, "id") == 0 && p->kind == GTOK_STRING) {
            n->id = strdup(p->str_val);
            gparse_advance(p);
        } else if (strcmp(key, "field_key") == 0 && p->kind == GTOK_STRING) {
            n->field_key = strdup(p->str_val);
            gparse_advance(p);
        } else if (strcmp(key, "op") == 0 && p->kind == GTOK_STRING) {
            n->op = strdup(p->str_val);
            gparse_advance(p);
        } else if (strcmp(key, "value") == 0 && p->kind == GTOK_STRING) {
            n->value = strdup(p->str_val);
            gparse_advance(p);
        } else if (strcmp(key, "layer") == 0 && p->kind == GTOK_NUMBER) {
            n->layer = (int)p->num_val;
            gparse_advance(p);
        } else if (strcmp(key, "stable_key") == 0 && p->kind == GTOK_STRING) {
            n->stable_key = strdup(p->str_val);
            gparse_advance(p);
        } else if (strcmp(key, "sub_category") == 0 && p->kind == GTOK_STRING) {
            n->sub_category = strdup(p->str_val);
            gparse_advance(p);
        } else if (strcmp(key, "domain") == 0 && p->kind == GTOK_STRING) {
            n->domain = strdup(p->str_val);
            gparse_advance(p);
        } else if (strcmp(key, "entity") == 0 && p->kind == GTOK_STRING) {
            n->entity = strdup(p->str_val);
            gparse_advance(p);
        } else if (strcmp(key, "children") == 0 && p->kind == GTOK_LBRACK) {
            n->children = parse_node_array(p, &n->child_count);
        } else if (strcmp(key, "do") == 0 && p->kind == GTOK_LBRACK) {
            n->do_nodes = parse_node_array(p, &n->do_count);
        } else {
            /* Skip unknown value */
            if (p->kind == GTOK_LBRACE || p->kind == GTOK_LBRACK) {
                int depth = 1;
                while (depth > 0 && p->kind != GTOK_EOF && p->kind != GTOK_ERROR) {
                    gparse_advance(p);
                    if (p->kind == GTOK_LBRACE || p->kind == GTOK_LBRACK) depth++;
                    if (p->kind == GTOK_RBRACE || p->kind == GTOK_RBRACK) depth--;
                }
                if (p->kind != GTOK_EOF) gparse_advance(p);
            } else {
                gparse_advance(p);
            }
        }

        free(key);
        if (p->kind == GTOK_COMMA) gparse_advance(p);
    }

    if (p->kind == GTOK_RBRACE)
        gparse_advance(p);

    return n;
}

/* ── Legacy "gates" array → DAG root converter ──────────────────────── */
static bs_gate_node_t* legacy_gates_to_dag(bs_gate_node_t** nodes, size_t count)
{
    if (count == 0) return NULL;
    if (count == 1) return nodes[0];

    bs_gate_node_t* root = (bs_gate_node_t*)calloc(1, sizeof(bs_gate_node_t));
    if (!root) return nodes[0];
    root->type = strdup("bs_gate_root");
    root->id   = strdup("_auto_root");
    root->children = nodes;
    root->child_count = count;
    return root;
}

/* ══════════════════════════════════════════════════════════════════════
 * bs_gate_chain_from_json — supports both DAG (root) and legacy (gates)
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

    gparse_advance(&p);
    if (p.kind != GTOK_LBRACE) { bs_gate_chain_free(chain); return -1; }
    gparse_advance(&p);

    while (p.kind == GTOK_STRING) {
        char* key = strdup(p.str_val);
        gparse_advance(&p);
        if (p.kind != GTOK_COLON) { free(key); bs_gate_chain_free(chain); return -1; }
        gparse_advance(&p);

        if (strcmp(key, "version") == 0) {
            if (p.kind == GTOK_STRING) {
                free(chain->version);
                chain->version = strdup(p.str_val);
            }
            gparse_advance(&p);
        } else if (strcmp(key, "root") == 0) {
            if (p.kind == GTOK_LBRACE) {
                chain->root = parse_node(&p);
            } else if (p.kind == GTOK_NULL) {
                chain->root = NULL;
                gparse_advance(&p);
            }
        } else if (strcmp(key, "gates") == 0 && p.kind == GTOK_LBRACK) {
            /* Legacy flat array format */
            gparse_advance(&p); /* skip [ */

            size_t cap = 16, cnt = 0;
            bs_gate_node_t** nodes = (bs_gate_node_t**)calloc(cap, sizeof(bs_gate_node_t*));

            while (p.kind == GTOK_LBRACE) {
                if (cnt >= cap) {
                    cap *= 2;
                    bs_gate_node_t** new_nodes = (bs_gate_node_t**)realloc(nodes, cap * sizeof(bs_gate_node_t*));
                    if (!new_nodes) break;
                    nodes = new_nodes;
                }
                nodes[cnt++] = parse_node(&p);
                if (p.kind == GTOK_COMMA) gparse_advance(&p);
            }

            if (p.kind != GTOK_RBRACK) { /* skip trailing */ }
            if (p.kind != GTOK_EOF) gparse_advance(&p);

            chain->root = legacy_gates_to_dag(nodes, cnt);
            /* Note: legacy_gates_to_dag takes ownership of the nodes array */
        } else {
            /* Skip unknown field */
            if (p.kind == GTOK_LBRACE || p.kind == GTOK_LBRACK) {
                int depth = 1;
                while (depth > 0 && p.kind != GTOK_EOF && p.kind != GTOK_ERROR) {
                    gparse_advance(&p);
                    if (p.kind == GTOK_LBRACE || p.kind == GTOK_LBRACK) depth++;
                    if (p.kind == GTOK_RBRACE || p.kind == GTOK_RBRACK) depth--;
                }
                if (p.kind != GTOK_EOF) gparse_advance(&p);
            } else {
                gparse_advance(&p);
            }
        }

        free(key);
        if (p.kind == GTOK_COMMA) gparse_advance(&p);
    }

    gparse_free(&p);

    *out = chain;
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Hash map (FNV-1a + open addressing, pointer-based)
 * ══════════════════════════════════════════════════════════════════════ */

#define kMapTombstone ((char*)1)

static uint64_t fnv1a_hash(const char* key)
{
    uint64_t h = 14695981039346656037ULL;
    while (*key) { h ^= (unsigned char)*key++; h *= 1099511628211ULL; }
    return h;
}

static inline bool SLOT_EMPTY(const bs_gate_map_slot_t s)
{
    return s.stable_key == NULL;
}
static inline bool SLOT_TOMB(const bs_gate_map_slot_t s)
{
    return s.stable_key == kMapTombstone;
}

static size_t next_pow2(size_t n)
{
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

static bs_gate_map_slot_t* map_find_slot(bs_gate_map_t* map,
                                          const char* stable_key,
                                          uint64_t hash)
{
    size_t mask = map->capacity - 1;
    size_t idx = (size_t)(hash & mask);
    bs_gate_map_slot_t* tombstone = NULL;

    for (size_t i = 0; i < map->capacity; i++) {
        bs_gate_map_slot_t* slot = &map->slots[idx];
        if (SLOT_EMPTY(*slot)) return tombstone ? tombstone : slot;
        if (SLOT_TOMB(*slot)) {
            if (!tombstone) tombstone = slot;
            idx = (idx + 1) & mask;
            continue;
        }
        if (strcmp(slot->stable_key, stable_key) == 0) return slot;
        idx = (idx + 1) & mask;
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
                               old_slots[i].node_ptr);
            free(old_slots[i].stable_key);
        }
    }
    free(old_slots);
    return 0;
}

int bs_gate_map_insert(bs_gate_map_t* map, const char* stable_key,
                        bs_gate_node_t* node_ptr)
{
    if (!map || !stable_key) return -1;

    if (map->count + 1 > map->capacity * 7 / 10) {
        if (bs_gate_map_rebuild(map) != 0) return -1;
    }

    uint64_t hash = fnv1a_hash(stable_key);
    bs_gate_map_slot_t* slot = map_find_slot(map, stable_key, hash);
    if (!slot) return -1;

    if (!SLOT_EMPTY(*slot) && !SLOT_TOMB(*slot)) {
        free(slot->stable_key);
    } else {
        map->count++;
    }
    slot->stable_key = strdup(stable_key);
    slot->node_ptr = node_ptr;
    return 0;
}

int bs_gate_map_lookup(const bs_gate_map_t* map, const char* stable_key,
                        bs_gate_node_t** out_ptr)
{
    if (!map || !stable_key || !out_ptr) return -1;
    if (map->count == 0) return -1;

    uint64_t hash = fnv1a_hash(stable_key);
    size_t mask = map->capacity - 1;
    size_t idx = (size_t)(hash & mask);

    for (size_t i = 0; i < map->capacity; i++) {
        const bs_gate_map_slot_t* slot = &map->slots[idx];
        if (SLOT_EMPTY(*slot)) return -1;
        if (!SLOT_TOMB(*slot) && strcmp(slot->stable_key, stable_key) == 0) {
            *out_ptr = slot->node_ptr;
            return 0;
        }
        idx = (idx + 1) & mask;
    }
    return -1;
}

/* ══════════════════════════════════════════════════════════════════════
 * bs_gate_chain_find — lookup by stable_key
 * ══════════════════════════════════════════════════════════════════════ */
bs_gate_node_t* bs_gate_chain_find(bs_gate_chain_t* chain, const char* stable_key)
{
    if (!chain || !stable_key || !chain->map) return NULL;
    bs_gate_node_t* ptr = NULL;
    if (bs_gate_map_lookup(chain->map, stable_key, &ptr) == 0)
        return ptr;
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════
 * bs_gate_chain_upsert_node — idempotent create or overwrite
 * ══════════════════════════════════════════════════════════════════════ */
static void copy_node_fields(bs_gate_node_t* dst, const bs_gate_node_t* src)
{
    free(dst->type);     dst->type     = NULL;
    free(dst->id);       dst->id       = NULL;
    free(dst->field_key); dst->field_key = NULL;
    free(dst->op);       dst->op       = NULL;
    free(dst->value);    dst->value    = NULL;
    free(dst->stable_key); dst->stable_key = NULL;
    free(dst->sub_category); dst->sub_category = NULL;
    free(dst->domain);   dst->domain   = NULL;
    free(dst->entity);   dst->entity   = NULL;

    if (src->type)       dst->type       = strdup(src->type);
    if (src->id)         dst->id         = strdup(src->id);
    if (src->field_key)  dst->field_key  = strdup(src->field_key);
    if (src->op)         dst->op         = strdup(src->op);
    if (src->value)      dst->value      = strdup(src->value);
    if (src->stable_key) dst->stable_key = strdup(src->stable_key);
    if (src->sub_category) dst->sub_category = strdup(src->sub_category);
    if (src->domain)     dst->domain     = strdup(src->domain);
    if (src->entity)     dst->entity     = strdup(src->entity);
    dst->layer = src->layer;
}

bs_gate_node_t* bs_gate_chain_upsert_node(bs_gate_chain_t* chain,
                                           const bs_gate_node_t* src)
{
    if (!chain || !src) return NULL;

    if (!chain->map) {
        bs_gate_map_create(&chain->map, 16);
    }

    if (src->stable_key) {
        bs_gate_node_t* existing = NULL;
        if (bs_gate_map_lookup(chain->map, src->stable_key, &existing) == 0 && existing) {
            copy_node_fields(existing, src);
            return existing;
        }
    }

    bs_gate_node_t* new_node = (bs_gate_node_t*)calloc(1, sizeof(bs_gate_node_t));
    if (!new_node) return NULL;
    copy_node_fields(new_node, src);

    if (new_node->stable_key) {
        bs_gate_map_insert(chain->map, new_node->stable_key, new_node);
    }

    if (!chain->root) {
        chain->root = new_node;
    }

    return new_node;
}
