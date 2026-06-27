/* Gate AST Compiler: JSON AST → pointer DAG.
 * Compiles 6 AST node types (condition/and/or/not/then/action) into
 * bs_gate_chain_t using the DAG pointer structures. */
#include <bs/kernel/gate_chain/gate_ast.h>
#include <bs/kernel/gate_chain/gate_chain_serialize.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Simple JSON tokenizer (self-contained, mirrors gate_chain_serialize.c) ── */
typedef enum {
    ASTOK_EOF, ASTOK_LBRACE, ASTOK_RBRACE, ASTOK_LBRACK, ASTOK_RBRACK,
    ASTOK_COMMA, ASTOK_COLON, ASTOK_STRING, ASTOK_NUMBER,
    ASTOK_TRUE, ASTOK_FALSE, ASTOK_NULL, ASTOK_ERROR
} astok_kind_t;

typedef struct {
    const char* pos;
    const char* tok_start;
    const char* end;
    astok_kind_t kind;
    char*       str_val;
    double      num_val;
} asparse_t;

static void asparse_init(asparse_t* p, const char* json)
{
    p->pos = json;
    p->end = json + strlen(json);
    p->str_val = NULL;
    p->tok_start = NULL;
}

static void asparse_free(asparse_t* p) { free(p->str_val); p->str_val = NULL; }

static void asparse_advance(asparse_t* p)
{
    free(p->str_val);
    p->str_val = NULL;
    p->tok_start = NULL;

    while (p->pos < p->end && (unsigned char)*p->pos <= ' ')
        p->pos++;
    if (p->pos >= p->end) { p->kind = ASTOK_EOF; return; }

    p->tok_start = p->pos;
    char c = *p->pos;
    switch (c) {
    case '{': p->pos++; p->kind = ASTOK_LBRACE; return;
    case '}': p->pos++; p->kind = ASTOK_RBRACE; return;
    case '[': p->pos++; p->kind = ASTOK_LBRACK; return;
    case ']': p->pos++; p->kind = ASTOK_RBRACK; return;
    case ',': p->pos++; p->kind = ASTOK_COMMA;  return;
    case ':': p->pos++; p->kind = ASTOK_COLON;  return;
    case '"': {
        p->pos++;
        const char* sstart = p->pos;
        size_t len = 0;
        while (p->pos < p->end && *p->pos != '"') {
            if (*p->pos == '\\' && p->pos + 1 < p->end) p->pos++;
            p->pos++; len++;
        }
        if (p->pos >= p->end) { p->kind = ASTOK_ERROR; return; }
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
        p->pos++; p->kind = ASTOK_STRING; return;
    }
    default:
        if (c == 't' && p->end - p->pos >= 4 && memcmp(p->pos, "true", 4) == 0)
        { p->pos += 4; p->kind = ASTOK_TRUE; return; }
        if (c == 'f' && p->end - p->pos >= 5 && memcmp(p->pos, "false", 5) == 0)
        { p->pos += 5; p->kind = ASTOK_FALSE; return; }
        if (c == 'n' && p->end - p->pos >= 4 && memcmp(p->pos, "null", 4) == 0)
        { p->pos += 4; p->kind = ASTOK_NULL; return; }
        if (c == '-' || (c >= '0' && c <= '9')) {
            char* end; p->num_val = strtod(p->pos, &end);
            if (end > p->pos) { p->pos = end; p->kind = ASTOK_NUMBER; return; }
        }
        p->kind = ASTOK_ERROR; return;
    }
}

/* ── Forward: recursive compile node ────────────────────────────────── */
static bs_gate_node_t* compile_ast_node(asparse_t* p, bs_gate_chain_t* chain);

/* Parse a string value from JSON token stream */
static char* parse_string_field(asparse_t* p, const char* expected_field)
{
    (void)expected_field;
    if (p->kind == ASTOK_STRING) {
        char* val = strdup(p->str_val);
        asparse_advance(p);
        return val;
    }
    return NULL;
}

/* parse_value_as_string: read any JSON value as string representation */
static char* parse_value_as_string(asparse_t* p)
{
    if (p->kind == ASTOK_STRING) {
        char* s = strdup(p->str_val);
        asparse_advance(p);
        return s;
    }
    if (p->kind == ASTOK_NUMBER) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%g", p->num_val);
        asparse_advance(p);
        return strdup(buf);
    }
    return NULL;
}

/* Parse a JSON obj and extract "type" field */
static char* peek_type(asparse_t* p)
{
    if (p->kind != ASTOK_LBRACE) return NULL;

    /* Save position + kind for backtrack */
    const char* saved_pos = p->pos;
    astok_kind_t saved_kind = p->kind;
    char* saved_str = p->str_val ? strdup(p->str_val) : NULL;

    asparse_advance(p); /* skip { */
    while (p->kind == ASTOK_STRING) {
        char* key = strdup(p->str_val);
        asparse_advance(p);
        if (p->kind != ASTOK_COLON) { free(key); break; }
        asparse_advance(p);

        if (strcmp(key, "type") == 0 && p->kind == ASTOK_STRING) {
            char* type = strdup(p->str_val);
            free(key);
            /* Restore position + kind */
            p->pos = saved_pos;
            p->kind = saved_kind;
            free(p->str_val); p->str_val = saved_str; saved_str = NULL;
            return type;
        }

        free(key);
        goto skip_val;
    }

skip_val:
    p->pos = saved_pos;
    p->kind = saved_kind;
    free(p->str_val); p->str_val = saved_str;
    return NULL;
}

/* ── Parse a dict key-value pair ────────────────────────────────────── */
typedef struct {
    char* type;
    char* field;
    char* op;
    char* value;
    char* name;
    /* For recursive: left/right/when/do compiled as sub-nodes */
    bs_gate_node_t* left;
    bs_gate_node_t* right;
    bs_gate_node_t* when;
    bs_gate_node_t* do_node;
    bs_gate_node_t* not_node;
} ast_fields_t;

static void ast_fields_init(ast_fields_t* f) { memset(f, 0, sizeof(*f)); }

static void ast_fields_free(ast_fields_t* f)
{
    free(f->type); free(f->field); free(f->op); free(f->value); free(f->name);
    /* Do NOT free left/right/when/do_node/not_node — they're owned by chain */
}

/* Parse all fields from a JSON object into ast_fields_t */
static void ast_parse_fields(asparse_t* p, ast_fields_t* f, bs_gate_chain_t* chain)
{
    if (p->kind != ASTOK_LBRACE) return;
    asparse_advance(p); /* skip { */

    while (p->kind == ASTOK_STRING) {
        char* key = strdup(p->str_val);
        asparse_advance(p);
        if (p->kind != ASTOK_COLON) { free(key); break; }
        asparse_advance(p);

        if (strcmp(key, "type") == 0) {
            f->type = parse_string_field(p, "type");
        } else if (strcmp(key, "field") == 0) {
            f->field = parse_string_field(p, "field");
        } else if (strcmp(key, "op") == 0) {
            f->op = parse_string_field(p, "op");
        } else if (strcmp(key, "value") == 0) {
            f->value = parse_value_as_string(p);
        } else if (strcmp(key, "name") == 0) {
            f->name = parse_string_field(p, "name");
        } else if (strcmp(key, "left") == 0 && p->kind == ASTOK_LBRACE) {
            f->left = compile_ast_node(p, chain);
        } else if (strcmp(key, "right") == 0 && p->kind == ASTOK_LBRACE) {
            f->right = compile_ast_node(p, chain);
        } else if (strcmp(key, "when") == 0 && p->kind == ASTOK_LBRACE) {
            f->when = compile_ast_node(p, chain);
        } else if (strcmp(key, "do") == 0 && p->kind == ASTOK_LBRACE) {
            f->do_node = compile_ast_node(p, chain);
        } else if (strcmp(key, "node") == 0 && p->kind == ASTOK_LBRACE) {
            f->not_node = compile_ast_node(p, chain);
        } else {
            /* Skip unknown value */
            if (p->kind == ASTOK_LBRACE || p->kind == ASTOK_LBRACK) {
                int depth = 1;
                while (depth > 0 && p->kind != ASTOK_EOF && p->kind != ASTOK_ERROR) {
                    asparse_advance(p);
                    if (p->kind == ASTOK_LBRACE || p->kind == ASTOK_LBRACK) depth++;
                    if (p->kind == ASTOK_RBRACE || p->kind == ASTOK_RBRACK) depth--;
                }
                if (p->kind != ASTOK_EOF) asparse_advance(p);
            } else {
                asparse_advance(p);
            }
        }

        free(key);
        if (p->kind == ASTOK_COMMA) asparse_advance(p);
    }

    if (p->kind == ASTOK_RBRACE) asparse_advance(p);
}

/* ── Recursive AST node compiler ────────────────────────────────────── */
/* Returns a bs_gate_node_t* that is owned by the chain.                */
static bs_gate_node_t* compile_ast_node(asparse_t* p, bs_gate_chain_t* chain)
{
    if (!p || p->kind != ASTOK_LBRACE) return NULL;

    /* Peek type first to know what we're building */
    char* node_type = peek_type(p);
    if (!node_type) {
        /* Can't determine type, skip */
        int depth = 1;
        while (depth > 0 && p->kind != ASTOK_EOF && p->kind != ASTOK_ERROR) {
            asparse_advance(p);
            if (p->kind == ASTOK_LBRACE || p->kind == ASTOK_LBRACK) depth++;
            if (p->kind == ASTOK_RBRACE || p->kind == ASTOK_RBRACK) depth--;
        }
        if (p->kind != ASTOK_EOF) asparse_advance(p);
        return NULL;
    }

    ast_fields_t f;
    ast_fields_init(&f);
    ast_parse_fields(p, &f, chain);

    bs_gate_node_t* node = NULL;

    if (strcmp(node_type, "condition") == 0) {
        /* condition: { type:"condition", field, op, value } */
        node = bs_gate_node_create("bs_condition", NULL);
        if (node) {
            node->field_key = f.field; f.field = NULL;
            node->op = f.op; f.op = NULL;
            node->value = f.value; f.value = NULL;
            node->layer = BS_GATE_LAYER_DEFAULT;
        }
    } else if (strcmp(node_type, "and") == 0) {
        /* and: { type:"and", left:{...}, right:{...} } */
        node = bs_gate_node_create("bs_logic_and", NULL);
        if (node && f.left) {
            bs_gate_node_link_child(node, f.left); f.left = NULL;
        }
        if (node && f.right) {
            bs_gate_node_link_child(node, f.right); f.right = NULL;
        }
    } else if (strcmp(node_type, "or") == 0) {
        /* or: { type:"or", left:{...}, right:{...} } */
        node = bs_gate_node_create("bs_logic_or", NULL);
        if (node && f.left) {
            bs_gate_node_link_child(node, f.left); f.left = NULL;
        }
        if (node && f.right) {
            bs_gate_node_link_child(node, f.right); f.right = NULL;
        }
    } else if (strcmp(node_type, "not") == 0) {
        /* not: { type:"not", node:{...} }
         * Compile as a condition that negates: not(cond) → we wrap in and+negate
         * For MVP: skip not (unsupported) */
        node = f.not_node; f.not_node = NULL;
        /* In full impl: wrap in a meta-rule with operator "not" */
    } else if (strcmp(node_type, "then") == 0) {
        /* then: { type:"then", when:{...}, do:{...} }
         * Compile: create a condition node with do branch */
        node = bs_gate_node_create("bs_condition", NULL);
        if (node && f.when) {
            /* The condition becomes a logical container:
             * For a simple "when" condition, copy scalar fields */
            bs_gate_node_t* when = f.when; f.when = NULL;
            if (when->field_key) { node->field_key = strdup(when->field_key); }
            if (when->op) { node->op = strdup(when->op); }
            if (when->value) { node->value = strdup(when->value); }
            /* Link when's children into node */
            for (size_t i = 0; i < when->child_count; i++)
                bs_gate_node_link_child(node, when->children[i]);
            when->child_count = 0;
            when->children = NULL;
            bs_gate_node_free(when);
        }
        if (node && f.do_node) {
            bs_gate_node_link_do(node, f.do_node); f.do_node = NULL;
        }
    } else if (strcmp(node_type, "action") == 0) {
        /* action: { type:"action", name, value } — store as meta_rule */
        node = bs_gate_node_create("bs_meta_rule", NULL);
        if (node) {
            node->field_key = f.name; f.name = NULL;
            node->value = f.value; f.value = NULL;
            node->op = strdup("action");
        }
    } else {
        /* Unknown type: create a pass-through node */
        node = bs_gate_node_create(node_type, NULL);
    }

    ast_fields_free(&f);
    free(node_type);

    /* Register node in chain */
    if (node && chain) {
        /* Generate stable_key for condition nodes */
        if (!node->stable_key && node->field_key && node->op) {
            char sk[256];
            snprintf(sk, sizeof(sk), "ast:%s:%s:%d:compiled",
                     node->field_key, node->op, node->layer);
            node->stable_key = strdup(sk);
        }
    }

    return node;
}

/* ══════════════════════════════════════════════════════════════════════
 * Public API
 * ══════════════════════════════════════════════════════════════════════ */

bs_gate_chain_t* bs_gate_ast_compile(const char* ast_json)
{
    if (!ast_json) return NULL;

    bs_gate_chain_t* chain = bs_gate_chain_create();
    if (!chain) return NULL;

    asparse_t p;
    asparse_init(&p, ast_json);

    asparse_advance(&p);

    /* Expect either a single object { type, ... } or an array [...] */
    if (p.kind == ASTOK_LBRACE) {
        chain->root = compile_ast_node(&p, chain);
    } else if (p.kind == ASTOK_LBRACK) {
        /* Array of nodes → wrap in logic_and */
        bs_gate_node_t* and_node = bs_gate_node_create("bs_gate_root", "_ast_root");
        asparse_advance(&p); /* skip [ */
        while (p.kind == ASTOK_LBRACE) {
            bs_gate_node_t* child = compile_ast_node(&p, chain);
            if (child) bs_gate_node_link_child(and_node, child);
            if (p.kind == ASTOK_COMMA) asparse_advance(&p);
        }
        if (p.kind == ASTOK_RBRACK) asparse_advance(&p);
        chain->root = and_node;
    }

    asparse_free(&p);
    return chain;
}

bs_gate_chain_t* bs_gate_ast_compile_and(const char** field_keys,
                                          const char** ops,
                                          const char** values,
                                          size_t count)
{
    bs_gate_chain_t* chain = bs_gate_chain_create();
    if (!chain) return NULL;

    if (count == 0) return chain;

    bs_gate_node_t* and_node = NULL;

    if (count == 1) {
        /* Single condition, no AND wrapper needed */
        bs_gate_node_t* cond = bs_gate_node_create("bs_condition", NULL);
        if (cond) {
            cond->field_key = strdup(field_keys[0]);
            cond->op = strdup(ops[0]);
            cond->value = strdup(values[0]);
            cond->layer = BS_GATE_LAYER_DEFAULT;
        }
        chain->root = cond;
        return chain;
    }

    and_node = bs_gate_node_create("bs_logic_and", "_auto_and");

    for (size_t i = 0; i < count; i++) {
        bs_gate_node_t* cond = bs_gate_node_create("bs_condition", NULL);
        if (!cond) continue;
        cond->field_key = strdup(field_keys[i]);
        cond->op = strdup(ops[i]);
        cond->value = strdup(values[i]);
        cond->layer = BS_GATE_LAYER_DEFAULT;
        bs_gate_node_link_child(and_node, cond);
    }

    chain->root = and_node;
    return chain;
}

bs_gate_chain_t* bs_gate_ast_compile_condition(const char* field_key,
                                                 const char* op,
                                                 const char* value)
{
    bs_gate_chain_t* chain = bs_gate_chain_create();
    if (!chain) return NULL;

    bs_gate_node_t* cond = bs_gate_node_create("bs_condition", NULL);
    if (cond) {
        cond->field_key = strdup(field_key);
        cond->op = strdup(op);
        cond->value = strdup(value);
        cond->layer = BS_GATE_LAYER_DEFAULT;
    }
    chain->root = cond;
    return chain;
}
