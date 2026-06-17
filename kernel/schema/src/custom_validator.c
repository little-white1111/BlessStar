#include <bs/kernel/schema/custom_validator.h>
#include "schema_compat.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════
 * Simple recursive-descent expression interpreter.
 *
 * Grammar (MVP):
 *   expr     → or_expr
 *   or_expr  → and_expr ( '||' and_expr )*
 *   and_expr → cmp_expr ( '&&' cmp_expr )*
 *   cmp_expr → arith_expr ( ('=='|'!='|'>'|'<'|'>='|'<=') arith_expr )?
 *   arith    → term ( ('+'|'-') term )*
 *   term     → unary ( ('*'|'/') unary )*
 *   unary    → '!' unary | primary
 *   primary  → NUMBER | STRING | 'true' | 'false' | 'value' | '(' expr ')'
 *
 * No operator precedence for * / in this MVP (left-to-right for + - only).
 * We keep it simple: only + - supported, no * / for MVP.
 * ═══════════════════════════════════════════════════════════════════════ */

/* ── Tokeniser types ───────────────────────────────────────────────── */
typedef enum
{
    TOK_EOF,
    TOK_NUMBER,
    TOK_STRING,
    TOK_TRUE,
    TOK_FALSE,
    TOK_VALUE,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_PLUS,
    TOK_MINUS,
    TOK_EQ,    /* == */
    TOK_NE,    /* != */
    TOK_GT,    /* >  */
    TOK_LT,    /* <  */
    TOK_GE,    /* >= */
    TOK_LE,    /* <= */
    TOK_AND,   /* && */
    TOK_OR,    /* || */
    TOK_NOT,   /* !  */
    TOK_ERROR
} token_kind_t;

typedef struct
{
    token_kind_t kind;
    double       num_val;
    char*        str_val; /* owned */
} token_t;

/* ── Lexer state ───────────────────────────────────────────────────── */
typedef struct
{
    const char* s;
    const char* pos;
    token_t     cur;
    int         err;
    char        err_msg[128];
} lexer_t;

static void lexer_init(lexer_t* l, const char* s)
{
    l->s   = s;
    l->pos = s;
    l->err = 0;
    l->err_msg[0] = '\0';
}

static void lexer_advance(lexer_t* l)
{
    /* Skip whitespace */
    while (*l->pos && (unsigned char)*l->pos <= ' ' && *l->pos != '\0')
        l->pos++;
    if (!*l->pos)
    {
        l->cur.kind = TOK_EOF;
        return;
    }

    char c = *l->pos;

    /* String literal (single-quoted) */
    if (c == '\'')
    {
        l->pos++;
        const char* start = l->pos;
        while (*l->pos && *l->pos != '\'') l->pos++;
        size_t len = (size_t)(l->pos - start);
        free(l->cur.str_val);
        l->cur.str_val = (char*)malloc(len + 1);
        if (l->cur.str_val)
        {
            memcpy(l->cur.str_val, start, len);
            l->cur.str_val[len] = '\0';
        }
        l->cur.kind = TOK_STRING;
        if (*l->pos == '\'') l->pos++;
        return;
    }

    /* Numbers (integer or float) */
    if (isdigit(c) || (c == '-' && isdigit(l->pos[1])))
    {
        char* end;
        l->cur.num_val = strtod(l->pos, &end);
        if (end == l->pos)
        {
            l->err = 1;
            snprintf(l->err_msg, sizeof(l->err_msg), "invalid number at '%s'", l->pos);
            l->cur.kind = TOK_ERROR;
            return;
        }
        l->pos = end;
        l->cur.kind = TOK_NUMBER;
        return;
    }

    /* Two-char operators */
    if (c == '=' && l->pos[1] == '=') { l->pos += 2; l->cur.kind = TOK_EQ; return; }
    if (c == '!' && l->pos[1] == '=') { l->pos += 2; l->cur.kind = TOK_NE; return; }
    if (c == '>' && l->pos[1] == '=') { l->pos += 2; l->cur.kind = TOK_GE; return; }
    if (c == '<' && l->pos[1] == '=') { l->pos += 2; l->cur.kind = TOK_LE; return; }
    if (c == '&' && l->pos[1] == '&') { l->pos += 2; l->cur.kind = TOK_AND; return; }
    if (c == '|' && l->pos[1] == '|') { l->pos += 2; l->cur.kind = TOK_OR; return; }

    /* One-char operators */
    if (c == '(') { l->pos++; l->cur.kind = TOK_LPAREN; return; }
    if (c == ')') { l->pos++; l->cur.kind = TOK_RPAREN; return; }
    if (c == '+') { l->pos++; l->cur.kind = TOK_PLUS; return; }
    if (c == '-') { l->pos++; l->cur.kind = TOK_MINUS; return; }
    if (c == '>') { l->pos++; l->cur.kind = TOK_GT; return; }
    if (c == '<') { l->pos++; l->cur.kind = TOK_LT; return; }
    if (c == '!') { l->pos++; l->cur.kind = TOK_NOT; return; }

    /* Keywords: 'value', 'true', 'false' */
    if (strncmp(l->pos, "value", 5) == 0 && !isalnum(l->pos[5]))
    {
        l->pos += 5;
        l->cur.kind = TOK_VALUE;
        return;
    }
    if (strncmp(l->pos, "true", 4) == 0 && !isalnum(l->pos[4]))
    {
        l->pos += 4;
        l->cur.kind = TOK_TRUE;
        return;
    }
    if (strncmp(l->pos, "false", 5) == 0 && !isalnum(l->pos[5]))
    {
        l->pos += 5;
        l->cur.kind = TOK_FALSE;
        return;
    }

    l->err = 1;
    snprintf(l->err_msg, sizeof(l->err_msg), "unexpected char '%c' at '%s'", c, l->pos);
    l->cur.kind = TOK_ERROR;
}

static token_t lexer_peek(lexer_t* l)
{
    return l->cur;
}

static token_t lexer_consume(lexer_t* l)
{
    token_t t = l->cur;
    lexer_advance(l);
    return t;
}

/* ── Parser + evaluator (returns double) ───────────────────────────── */
/* We evaluate as double; 0.0 = false, non-zero = true. */

static double eval_expr(lexer_t* l, const bs_value_t* val, int* err, char* err_buf, size_t err_sz);
static double eval_or(lexer_t* l, const bs_value_t* val, int* err, char* err_buf, size_t err_sz);
static double eval_and(lexer_t* l, const bs_value_t* val, int* err, char* err_buf, size_t err_sz);
static double eval_cmp(lexer_t* l, const bs_value_t* val, int* err, char* err_buf, size_t err_sz);
static double eval_arith(lexer_t* l, const bs_value_t* val, int* err, char* err_buf, size_t err_sz);
static double eval_term(lexer_t* l, const bs_value_t* val, int* err, char* err_buf, size_t err_sz);
static double eval_unary(lexer_t* l, const bs_value_t* val, int* err, char* err_buf, size_t err_sz);
static double eval_primary(lexer_t* l, const bs_value_t* val, int* err, char* err_buf, size_t err_sz);

/* ── Value extraction helpers ──────────────────────────────────────── */
static double value_to_number(const bs_value_t* v)
{
    if (!v || v->type == BS_VAL_NULL) return 0.0;
    switch (v->type)
    {
    case BS_VAL_I32:  return (double)v->data.i32_val;
    case BS_VAL_I64:  return (double)v->data.i64_val;
    case BS_VAL_F64:  return v->data.f64_val;
    case BS_VAL_BOOL: return v->data.bool_val ? 1.0 : 0.0;
    case BS_VAL_STR:
    {
        char* end;
        return strtod(v->data.str_val, &end);
    }
    default: return 0.0;
    }
}

static const char* value_to_string(const bs_value_t* v)
{
    if (!v || v->type != BS_VAL_STR) return "";
    return v->data.str_val;
}

/* ══════════════════════════════════════════════════════════════════════ */
/* Exported entry point                                                  */
/* ══════════════════════════════════════════════════════════════════════ */
int bs_custom_validator_eval_expr(const char* expr,
                                   const bs_value_t* val,
                                   char* err_buf, size_t err_sz)
{
    if (!expr || !val) return 0;

    lexer_t l;
    lexer_init(&l, expr);
    lexer_advance(&l);

    int err = 0;
    double result = eval_expr(&l, val, &err, err_buf, err_sz);

    /* Check for trailing tokens */
    if (!err && l.cur.kind != TOK_EOF)
    {
        if (err_buf && err_sz > 0)
            snprintf(err_buf, err_sz, "unexpected trailing tokens after expression");
        return -1;
    }

    if (err)
        return -1; /* error message already set */

    return (result != 0.0) ? 1 : 0;
}

/* ══════════════════════════════════════════════════════════════════════ */
/* Recursive descent evaluators (all return double, 0=invalid)           */
/* ══════════════════════════════════════════════════════════════════════ */

static double eval_expr(lexer_t* l, const bs_value_t* val,
                         int* err, char* err_buf, size_t err_sz)
{
    return eval_or(l, val, err, err_buf, err_sz);
}

static double eval_or(lexer_t* l, const bs_value_t* val,
                       int* err, char* err_buf, size_t err_sz)
{
    double left = eval_and(l, val, err, err_buf, err_sz);
    if (*err) return 0;
    while (l->cur.kind == TOK_OR)
    {
        lexer_consume(l);
        double right = eval_and(l, val, err, err_buf, err_sz);
        if (*err) return 0;
        left = (left != 0.0 || right != 0.0) ? 1.0 : 0.0;
    }
    return left;
}

static double eval_and(lexer_t* l, const bs_value_t* val,
                        int* err, char* err_buf, size_t err_sz)
{
    double left = eval_cmp(l, val, err, err_buf, err_sz);
    if (*err) return 0;
    while (l->cur.kind == TOK_AND)
    {
        lexer_consume(l);
        double right = eval_cmp(l, val, err, err_buf, err_sz);
        if (*err) return 0;
        left = (left != 0.0 && right != 0.0) ? 1.0 : 0.0;
    }
    return left;
}

static double eval_cmp(lexer_t* l, const bs_value_t* val,
                        int* err, char* err_buf, size_t err_sz)
{
    double left = eval_arith(l, val, err, err_buf, err_sz);
    if (*err) return 0;

    token_kind_t op = l->cur.kind;
    if (op == TOK_EQ || op == TOK_NE || op == TOK_GT ||
        op == TOK_LT || op == TOK_GE || op == TOK_LE)
    {
        lexer_consume(l);
        double right = eval_arith(l, val, err, err_buf, err_sz);
        if (*err) return 0;
        switch (op)
        {
        case TOK_EQ: return (left == right) ? 1.0 : 0.0;
        case TOK_NE: return (left != right) ? 1.0 : 0.0;
        case TOK_GT: return (left > right)  ? 1.0 : 0.0;
        case TOK_LT: return (left < right)  ? 1.0 : 0.0;
        case TOK_GE: return (left >= right) ? 1.0 : 0.0;
        case TOK_LE: return (left <= right) ? 1.0 : 0.0;
        default: break;
        }
    }
    return left;
}

static double eval_arith(lexer_t* l, const bs_value_t* val,
                          int* err, char* err_buf, size_t err_sz)
{
    double left = eval_term(l, val, err, err_buf, err_sz);
    if (*err) return 0;
    while (l->cur.kind == TOK_PLUS || l->cur.kind == TOK_MINUS)
    {
        token_kind_t op = l->cur.kind;
        lexer_consume(l);
        double right = eval_term(l, val, err, err_buf, err_sz);
        if (*err) return 0;
        left = (op == TOK_PLUS) ? (left + right) : (left - right);
    }
    return left;
}

static double eval_term(lexer_t* l, const bs_value_t* val,
                         int* err, char* err_buf, size_t err_sz)
{
    return eval_unary(l, val, err, err_buf, err_sz);
}

static double eval_unary(lexer_t* l, const bs_value_t* val,
                          int* err, char* err_buf, size_t err_sz)
{
    if (l->cur.kind == TOK_NOT)
    {
        lexer_consume(l);
        double inner = eval_unary(l, val, err, err_buf, err_sz);
        if (*err) return 0;
        return (inner == 0.0) ? 1.0 : 0.0;
    }
    return eval_primary(l, val, err, err_buf, err_sz);
}

static double eval_primary(lexer_t* l, const bs_value_t* val,
                            int* err, char* err_buf, size_t err_sz)
{
    token_t t = lexer_consume(l);
    switch (t.kind)
    {
    case TOK_NUMBER:
        return t.num_val;

    case TOK_STRING:
    {
        /* Compare string to value */
        if (!val) return 0.0;
        const char* vs = value_to_string(val);
        return (strcmp(t.str_val, vs) == 0) ? 1.0 : 0.0;
    }

    case TOK_TRUE:
        return 1.0;

    case TOK_FALSE:
        return 0.0;

    case TOK_VALUE:
        return value_to_number(val);

    case TOK_LPAREN:
    {
        double inner = eval_expr(l, val, err, err_buf, err_sz);
        if (*err) return 0;
        if (l->cur.kind != TOK_RPAREN)
        {
            if (err_buf && err_sz > 0)
                snprintf(err_buf, err_sz, "missing ')'");
            *err = 1;
            return 0;
        }
        lexer_consume(l);
        return inner;
    }

    case TOK_MINUS:
    {
        /* Unary minus */
        return -eval_primary(l, val, err, err_buf, err_sz);
    }

    default:
        if (err_buf && err_sz > 0)
            snprintf(err_buf, err_sz, "unexpected token in expression");
        *err = 1;
        return 0;
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * Global C validator registry (simple, non-thread-safe)
 * ══════════════════════════════════════════════════════════════════════ */

#define BS_CUSTOM_VALIDATOR_GLOBAL_MAX 64

static struct
{
    char*               name;
    bs_custom_validator_fn fn;
    int                 used;
} s_global_cfn_table[BS_CUSTOM_VALIDATOR_GLOBAL_MAX];
static int s_global_cfn_count = 0;

int bs_custom_validator_global_register(const char* name,
                                         bs_custom_validator_fn fn)
{
    if (!name || !fn) return BS_SCHEMA_ERR_INVALID_ARG;
    if (s_global_cfn_count >= BS_CUSTOM_VALIDATOR_GLOBAL_MAX)
        return BS_SCHEMA_ERR_NO_MEMORY;
    for (int i = 0; i < s_global_cfn_count; i++)
    {
        if (strcmp(s_global_cfn_table[i].name, name) == 0)
            return BS_SCHEMA_ERR_ALREADY_EXISTS;
    }
    s_global_cfn_table[s_global_cfn_count].name = bs_strdup(name);
    if (!s_global_cfn_table[s_global_cfn_count].name)
        return BS_SCHEMA_ERR_NO_MEMORY;
    s_global_cfn_table[s_global_cfn_count].fn    = fn;
    s_global_cfn_table[s_global_cfn_count].used  = 1;
    s_global_cfn_count++;
    return BS_SCHEMA_OK;
}

int bs_custom_validator_global_find(const char* name,
                                     bs_custom_validator_fn* out_fn)
{
    if (!name) return BS_SCHEMA_ERR_INVALID_ARG;
    for (int i = 0; i < s_global_cfn_count; i++)
    {
        if (s_global_cfn_table[i].used &&
            strcmp(s_global_cfn_table[i].name, name) == 0)
        {
            if (out_fn) *out_fn = s_global_cfn_table[i].fn;
            return BS_SCHEMA_OK;
        }
    }
    return BS_SCHEMA_ERR_NOT_FOUND;
}
