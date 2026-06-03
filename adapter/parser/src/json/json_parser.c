#include "bs/adapter/parser/config_parse_status.h"
#include "bs/adapter/parser/json_lexer.h"
#include "bs/adapter/parser/json_parser.h"
#include "bs/adapter/parser/json_utf8.h"

#include <stdlib.h>
#include <string.h>

#define BS_PARSE_MAX_KEYS_SEEN 64u

typedef struct ParseCtx
{
    JsonLexer* lex;
    size_t*    error_line;
    size_t*    error_column;
    unsigned   depth;
    int        err_code;
} ParseCtx;

static void set_err(ParseCtx* ctx, int code, size_t line, size_t column)
{
    ctx->err_code = code;
    if (ctx->error_line)
        *ctx->error_line = line;
    if (ctx->error_column)
        *ctx->error_column = column;
}

typedef struct KeysSeen
{
    char*  keys[BS_PARSE_MAX_KEYS_SEEN];
    size_t count;
} KeysSeen;

static void keys_seen_clear(KeysSeen* ks)
{
    if (!ks)
        return;
    for (size_t i = 0; i < ks->count; ++i)
        free(ks->keys[i]);
    ks->count = 0;
}

static int keys_seen_add(KeysSeen* ks, const char* key, ParseCtx* ctx, size_t line, size_t column)
{
    for (size_t i = 0; i < ks->count; ++i)
    {
        if (strcmp(ks->keys[i], key) == 0)
        {
            set_err(ctx, BS_CONFIG_PARSE_ERR_SCHEMA, line, column);
            return 0;
        }
    }
    if (ks->count >= BS_PARSE_MAX_KEYS_SEEN)
    {
        set_err(ctx, BS_CONFIG_PARSE_ERR_SCHEMA, line, column);
        return 0;
    }
    const size_t key_len = strlen(key);
    char*        dup     = (char*)malloc(key_len + 1);
    if (!dup)
    {
        set_err(ctx, BS_CONFIG_PARSE_ERR_OOM, 0, 0);
        return 0;
    }
    memcpy(dup, key, key_len + 1);
    ks->keys[ks->count++] = dup;
    return 1;
}

static void metadata_list_destroy(ConfigV1Metadata* meta)
{
    while (meta)
    {
        ConfigV1Metadata* next = meta->next;
        free(meta->key);
        free(meta->value);
        free(meta);
        meta = next;
    }
}

static BsStatus fail_status(ParseCtx* ctx)
{
    return bs_status_from_config_parse(ctx->err_code ? ctx->err_code : BS_CONFIG_PARSE_ERR_PARSE);
}

static int hex_val(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return 10 + (c - 'a');
    return 10 + (c - 'A');
}

static char* decode_json_string(const JsonToken* tok)
{
    if (!tok || tok->type != JSON_TOK_STRING || tok->length < 2)
        return NULL;

    const char*  s   = tok->start + 1;
    const char*  e   = tok->start + tok->length - 1;
    const size_t cap = BS_JSON_MAX_STRING_BYTES + 1u;
    char*        out = (char*)malloc(cap);
    if (!out)
        return NULL;

    size_t o = 0;
    while (s < e)
    {
        if (o >= BS_JSON_MAX_STRING_BYTES)
        {
            free(out);
            return NULL;
        }
        if (*s != '\\')
        {
            if ((unsigned char)*s >= 0x80u)
            {
                const char*  p = s;
                unsigned int cp;
                if (!bs_json_utf8_decode_advance(&p, e, &cp))
                {
                    free(out);
                    return NULL;
                }
                while (s < p)
                {
                    if (o >= BS_JSON_MAX_STRING_BYTES)
                    {
                        free(out);
                        return NULL;
                    }
                    out[o++] = *s++;
                }
                continue;
            }
            out[o++] = *s++;
            continue;
        }
        ++s;
        if (s >= e)
        {
            free(out);
            return NULL;
        }
        switch (*s)
        {
        case '"':
        case '\\':
        case '/':
            out[o++] = *s++;
            break;
        case 'b':
            out[o++] = '\b';
            ++s;
            break;
        case 'f':
            out[o++] = '\f';
            ++s;
            break;
        case 'n':
            out[o++] = '\n';
            ++s;
            break;
        case 'r':
            out[o++] = '\r';
            ++s;
            break;
        case 't':
            out[o++] = '\t';
            ++s;
            break;
        case 'u':
            ++s;
            if (s + 4 > e)
            {
                free(out);
                return NULL;
            }
            {
                unsigned int cp = 0;
                for (int i = 0; i < 4; ++i)
                {
                    const char c = s[i];
                    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                          (c >= 'A' && c <= 'F')))
                    {
                        free(out);
                        return NULL;
                    }
                    cp = (cp << 4) + (unsigned)hex_val(c);
                }
                s += 4;
                if (!bs_json_utf8_codepoint_valid(cp))
                {
                    free(out);
                    return NULL;
                }
                if (cp <= 0x7Fu)
                {
                    if (o + 1 > BS_JSON_MAX_STRING_BYTES)
                    {
                        free(out);
                        return NULL;
                    }
                    out[o++] = (char)cp;
                }
                else if (cp <= 0x7FFu)
                {
                    if (o + 2 > BS_JSON_MAX_STRING_BYTES)
                    {
                        free(out);
                        return NULL;
                    }
                    out[o++] = (char)(0xC0u | (cp >> 6));
                    out[o++] = (char)(0x80u | (cp & 0x3Fu));
                }
                else
                {
                    if (o + 3 > BS_JSON_MAX_STRING_BYTES)
                    {
                        free(out);
                        return NULL;
                    }
                    out[o++] = (char)(0xE0u | (cp >> 12));
                    out[o++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
                    out[o++] = (char)(0x80u | (cp & 0x3Fu));
                }
            }
            break;
        default:
            free(out);
            return NULL;
        }
    }
    char* shrunk = (char*)realloc(out, o + 1);
    if (!shrunk)
    {
        free(out);
        return NULL;
    }
    shrunk[o] = '\0';
    return shrunk;
}

static const JsonToken* peek(ParseCtx* ctx)
{
    return bs_json_lexer_current(ctx->lex);
}

static int enter_container(ParseCtx* ctx)
{
    const JsonToken* tok = peek(ctx);
    if (ctx->depth >= BS_JSON_MAX_DEPTH)
    {
        set_err(ctx, BS_CONFIG_PARSE_ERR_SCHEMA, tok ? tok->line : 0, tok ? tok->column : 0);
        return 0;
    }
    ++ctx->depth;
    return 1;
}

static void leave_container(ParseCtx* ctx)
{
    if (ctx->depth > 0)
        --ctx->depth;
}

static const JsonToken* advance(ParseCtx* ctx);
static int              expect(ParseCtx* ctx, JsonTokenType type);

static int skip_json_value(ParseCtx* ctx);

static int skip_json_object(ParseCtx* ctx)
{
    if (!enter_container(ctx) || !expect(ctx, JSON_TOK_LBRACE))
        return 0;

    const JsonToken* tok = peek(ctx);
    if (tok && tok->type == JSON_TOK_RBRACE)
    {
        advance(ctx);
        leave_container(ctx);
        return 1;
    }

    while (1)
    {
        if (!expect(ctx, JSON_TOK_STRING))
            return 0;
        if (!expect(ctx, JSON_TOK_COLON))
            return 0;
        if (!skip_json_value(ctx))
            return 0;

        tok = peek(ctx);
        if (!tok)
            return 0;
        if (tok->type == JSON_TOK_COMMA)
        {
            advance(ctx);
            continue;
        }
        if (tok->type == JSON_TOK_RBRACE)
        {
            advance(ctx);
            leave_container(ctx);
            return 1;
        }
        set_err(ctx, BS_CONFIG_PARSE_ERR_PARSE, tok->line, tok->column);
        return 0;
    }
}

static int skip_json_array(ParseCtx* ctx)
{
    if (!enter_container(ctx) || !expect(ctx, JSON_TOK_LBRACKET))
        return 0;

    const JsonToken* tok = peek(ctx);
    if (tok && tok->type == JSON_TOK_RBRACKET)
    {
        advance(ctx);
        leave_container(ctx);
        return 1;
    }

    while (1)
    {
        if (!skip_json_value(ctx))
            return 0;

        tok = peek(ctx);
        if (!tok)
            return 0;
        if (tok->type == JSON_TOK_COMMA)
        {
            advance(ctx);
            continue;
        }
        if (tok->type == JSON_TOK_RBRACKET)
        {
            advance(ctx);
            leave_container(ctx);
            return 1;
        }
        set_err(ctx, BS_CONFIG_PARSE_ERR_PARSE, tok->line, tok->column);
        return 0;
    }
}

static int skip_json_value(ParseCtx* ctx)
{
    const JsonToken* tok = peek(ctx);
    if (!tok)
        return 0;
    switch (tok->type)
    {
    case JSON_TOK_STRING:
    case JSON_TOK_NUMBER:
    case JSON_TOK_TRUE:
    case JSON_TOK_FALSE:
    case JSON_TOK_NULL:
        advance(ctx);
        return 1;
    case JSON_TOK_LBRACE:
        return skip_json_object(ctx);
    case JSON_TOK_LBRACKET:
        return skip_json_array(ctx);
    default:
        set_err(ctx, BS_CONFIG_PARSE_ERR_SCHEMA, tok->line, tok->column);
        return 0;
    }
}

static const JsonToken* advance(ParseCtx* ctx)
{
    const JsonToken* prev = bs_json_lexer_current(ctx->lex);
    bs_json_lexer_next(ctx->lex);
    if (prev && prev->type == JSON_TOK_ERROR)
        set_err(ctx, BS_CONFIG_PARSE_ERR_LEX, prev->line, prev->column);
    return prev;
}

static int expect(ParseCtx* ctx, JsonTokenType type)
{
    const JsonToken* tok = peek(ctx);
    if (!tok || tok->type != type)
    {
        if (tok)
            set_err(ctx, BS_CONFIG_PARSE_ERR_PARSE, tok->line, tok->column);
        else
            set_err(ctx, BS_CONFIG_PARSE_ERR_PARSE, 0, 0);
        return 0;
    }
    advance(ctx);
    return 1;
}

static char* parse_string_value(ParseCtx* ctx)
{
    const JsonToken* tok = peek(ctx);
    if (!tok || tok->type != JSON_TOK_STRING)
    {
        if (tok)
            set_err(ctx, BS_CONFIG_PARSE_ERR_SCHEMA, tok->line, tok->column);
        return NULL;
    }
    char* decoded = decode_json_string(tok);
    if (!decoded)
        set_err(ctx, BS_CONFIG_PARSE_ERR_PARSE, tok->line, tok->column);
    advance(ctx);
    return decoded;
}

static int parse_metadata_object(ParseCtx* ctx, ConfigV1Metadata** out_head)
{
    if (!enter_container(ctx) || !expect(ctx, JSON_TOK_LBRACE))
        return 0;
    ConfigV1Metadata* head = NULL;
    ConfigV1Metadata* tail = NULL;
    KeysSeen          seen = {0};

    const JsonToken* tok = peek(ctx);
    if (tok && tok->type == JSON_TOK_RBRACE)
    {
        advance(ctx);
        leave_container(ctx);
        *out_head = NULL;
        return 1;
    }

    while (1)
    {
        const JsonToken* key_tok = peek(ctx);
        char*            key     = parse_string_value(ctx);
        if (!key || key[0] == '\0')
        {
            keys_seen_clear(&seen);
            free(key);
            return 0;
        }
        if (!keys_seen_add(&seen, key, ctx, key_tok ? key_tok->line : 0,
                           key_tok ? key_tok->column : 0))
        {
            keys_seen_clear(&seen);
            metadata_list_destroy(head);
            free(key);
            return 0;
        }
        if (!expect(ctx, JSON_TOK_COLON))
        {
            keys_seen_clear(&seen);
            metadata_list_destroy(head);
            free(key);
            return 0;
        }
        char* val = parse_string_value(ctx);
        if (!val)
        {
            keys_seen_clear(&seen);
            metadata_list_destroy(head);
            free(key);
            return 0;
        }
        ConfigV1Metadata* node = (ConfigV1Metadata*)calloc(1, sizeof(ConfigV1Metadata));
        if (!node)
        {
            keys_seen_clear(&seen);
            metadata_list_destroy(head);
            free(key);
            free(val);
            set_err(ctx, BS_CONFIG_PARSE_ERR_OOM, 0, 0);
            return 0;
        }
        node->key   = key;
        node->value = val;
        if (!head)
            head = node;
        else
            tail->next = node;
        tail = node;

        tok = peek(ctx);
        if (!tok)
        {
            keys_seen_clear(&seen);
            metadata_list_destroy(head);
            return 0;
        }
        if (tok->type == JSON_TOK_COMMA)
        {
            advance(ctx);
            continue;
        }
        if (tok->type == JSON_TOK_RBRACE)
        {
            advance(ctx);
            leave_container(ctx);
            keys_seen_clear(&seen);
            *out_head = head;
            return 1;
        }
        keys_seen_clear(&seen);
        metadata_list_destroy(head);
        set_err(ctx, BS_CONFIG_PARSE_ERR_PARSE, tok->line, tok->column);
        return 0;
    }
}

static int parse_instruction_item(ParseCtx* ctx, ConfigV1Instruction** out_instr)
{
    if (!enter_container(ctx) || !expect(ctx, JSON_TOK_LBRACE))
        return 0;

    ConfigV1Instruction* instr = (ConfigV1Instruction*)calloc(1, sizeof(ConfigV1Instruction));
    if (!instr)
    {
        set_err(ctx, BS_CONFIG_PARSE_ERR_OOM, 0, 0);
        return 0;
    }

    int              saw_type     = 0;
    int              saw_name     = 0;
    int              saw_metadata = 0;
    const JsonToken* tok          = peek(ctx);
    if (tok && tok->type == JSON_TOK_RBRACE)
    {
        set_err(ctx, BS_CONFIG_PARSE_ERR_SCHEMA, tok->line, tok->column);
        free(instr);
        return 0;
    }

    while (1)
    {
        const JsonToken* key_tok = peek(ctx);
        char*            key     = parse_string_value(ctx);
        if (!key)
        {
            free(instr);
            return 0;
        }
        if (!expect(ctx, JSON_TOK_COLON))
        {
            free(key);
            free(instr);
            return 0;
        }

        if (strcmp(key, "type") == 0)
        {
            if (saw_type)
            {
                set_err(ctx, BS_CONFIG_PARSE_ERR_SCHEMA, key_tok ? key_tok->line : 0,
                        key_tok ? key_tok->column : 0);
                free(key);
                free(instr->type);
                free(instr->name);
                free(instr);
                return 0;
            }
            instr->type = parse_string_value(ctx);
            saw_type    = instr->type != NULL;
        }
        else if (strcmp(key, "name") == 0)
        {
            if (saw_name)
            {
                set_err(ctx, BS_CONFIG_PARSE_ERR_SCHEMA, key_tok ? key_tok->line : 0,
                        key_tok ? key_tok->column : 0);
                free(key);
                free(instr->type);
                free(instr->name);
                free(instr);
                return 0;
            }
            instr->name = parse_string_value(ctx);
            saw_name    = instr->name != NULL;
        }
        else if (strcmp(key, "metadata") == 0)
        {
            if (saw_metadata)
            {
                set_err(ctx, BS_CONFIG_PARSE_ERR_SCHEMA, key_tok ? key_tok->line : 0,
                        key_tok ? key_tok->column : 0);
                free(key);
                free(instr->type);
                free(instr->name);
                free(instr);
                return 0;
            }
            saw_metadata = 1;
            tok          = peek(ctx);
            if (!tok || tok->type != JSON_TOK_LBRACE)
            {
                set_err(ctx, BS_CONFIG_PARSE_ERR_SCHEMA, tok ? tok->line : 0,
                        tok ? tok->column : 0);
                free(key);
                free(instr->type);
                free(instr->name);
                free(instr);
                return 0;
            }
            if (!parse_metadata_object(ctx, &instr->metadata))
            {
                free(key);
                free(instr->type);
                free(instr->name);
                metadata_list_destroy(instr->metadata);
                instr->metadata = NULL;
                free(instr);
                return 0;
            }
        }
        else
        {
            set_err(ctx, BS_CONFIG_PARSE_ERR_SCHEMA, key_tok ? key_tok->line : 0,
                    key_tok ? key_tok->column : 0);
            (void)skip_json_value(ctx);
            free(key);
            free(instr->type);
            free(instr->name);
            free(instr);
            return 0;
        }
        free(key);

        tok = peek(ctx);
        if (!tok)
        {
            free(instr);
            return 0;
        }
        if (tok->type == JSON_TOK_COMMA)
        {
            advance(ctx);
            continue;
        }
        if (tok->type == JSON_TOK_RBRACE)
        {
            advance(ctx);
            leave_container(ctx);
            break;
        }
        set_err(ctx, BS_CONFIG_PARSE_ERR_PARSE, tok->line, tok->column);
        free(instr);
        return 0;
    }

    if (!saw_type || !saw_name || !instr->type[0] || !instr->name[0])
    {
        set_err(ctx, BS_CONFIG_PARSE_ERR_SCHEMA, 0, 0);
        free(instr->type);
        free(instr->name);
        free(instr);
        return 0;
    }
    *out_instr = instr;
    return 1;
}

static void destroy_metadata_chain(ConfigV1Metadata* meta)
{
    while (meta)
    {
        ConfigV1Metadata* next = meta->next;
        free(meta->key);
        free(meta->value);
        free(meta);
        meta = next;
    }
}

static void free_instruction_list(ConfigV1Instruction* head)
{
    while (head)
    {
        ConfigV1Instruction* next = head->next;
        free(head->type);
        free(head->name);
        destroy_metadata_chain(head->metadata);
        free(head);
        head = next;
    }
}

static int parse_instructions_array(ParseCtx* ctx, ConfigV1Instruction** out_head,
                                    size_t* out_count)
{
    if (!enter_container(ctx) || !expect(ctx, JSON_TOK_LBRACKET))
        return 0;

    ConfigV1Instruction* head  = NULL;
    ConfigV1Instruction* tail  = NULL;
    size_t               count = 0;

    const JsonToken* tok = peek(ctx);
    if (tok && tok->type == JSON_TOK_RBRACKET)
    {
        advance(ctx);
        leave_container(ctx);
        *out_head  = NULL;
        *out_count = 0;
        return 1;
    }

    while (1)
    {
        if (count >= BS_JSON_MAX_INSTRUCTIONS)
        {
            const JsonToken* lim = peek(ctx);
            set_err(ctx, BS_CONFIG_PARSE_ERR_SCHEMA, lim ? lim->line : 0, lim ? lim->column : 0);
            free_instruction_list(head);
            leave_container(ctx);
            return 0;
        }
        ConfigV1Instruction* instr = NULL;
        if (!parse_instruction_item(ctx, &instr))
        {
            free_instruction_list(head);
            leave_container(ctx);
            return 0;
        }
        if (!head)
            head = instr;
        else
            tail->next = instr;
        tail = instr;
        ++count;

        tok = peek(ctx);
        if (!tok)
            return 0;
        if (tok->type == JSON_TOK_COMMA)
        {
            advance(ctx);
            continue;
        }
        if (tok->type == JSON_TOK_RBRACKET)
        {
            advance(ctx);
            leave_container(ctx);
            *out_head  = head;
            *out_count = count;
            return 1;
        }
        set_err(ctx, BS_CONFIG_PARSE_ERR_PARSE, tok->line, tok->column);
        leave_container(ctx);
        return 0;
    }
}

static int parse_manual_array(ParseCtx* ctx, char*** out_items, size_t* out_count)
{
    if (!enter_container(ctx) || !expect(ctx, JSON_TOK_LBRACKET))
        return 0;

    size_t cap   = 4;
    size_t count = 0;
    char** items = (char**)calloc(cap, sizeof(char*));
    if (!items)
    {
        set_err(ctx, BS_CONFIG_PARSE_ERR_OOM, 0, 0);
        return 0;
    }

    const JsonToken* tok = peek(ctx);
    if (tok && tok->type == JSON_TOK_RBRACKET)
    {
        advance(ctx);
        leave_container(ctx);
        *out_items = items;
        *out_count = 0;
        return 1;
    }

    while (1)
    {
        if (count >= BS_JSON_MAX_MANUAL_ITEMS)
        {
            const JsonToken* lim = peek(ctx);
            set_err(ctx, BS_CONFIG_PARSE_ERR_SCHEMA, lim ? lim->line : 0, lim ? lim->column : 0);
            for (size_t i = 0; i < count; ++i)
                free(items[i]);
            free(items);
            leave_container(ctx);
            return 0;
        }
        char* s = parse_string_value(ctx);
        if (!s || s[0] == '\0')
        {
            for (size_t i = 0; i < count; ++i)
                free(items[i]);
            free(items);
            free(s);
            return 0;
        }
        if (count >= cap)
        {
            cap *= 2;
            char** grown = (char**)realloc(items, cap * sizeof(char*));
            if (!grown)
            {
                free(s);
                set_err(ctx, BS_CONFIG_PARSE_ERR_OOM, 0, 0);
                return 0;
            }
            items = grown;
        }
        items[count++] = s;

        tok = peek(ctx);
        if (!tok)
        {
            for (size_t i = 0; i < count; ++i)
                free(items[i]);
            free(items);
            return 0;
        }
        if (tok->type == JSON_TOK_COMMA)
        {
            advance(ctx);
            continue;
        }
        if (tok->type == JSON_TOK_RBRACKET)
        {
            advance(ctx);
            leave_container(ctx);
            *out_items = items;
            *out_count = count;
            return 1;
        }
        set_err(ctx, BS_CONFIG_PARSE_ERR_PARSE, tok->line, tok->column);
        for (size_t i = 0; i < count; ++i)
            free(items[i]);
        free(items);
        leave_container(ctx);
        return 0;
    }
}

static int is_known_root_key(const char* key)
{
    return strcmp(key, "kernel_version") == 0 || strcmp(key, "adapter_version") == 0 ||
           strcmp(key, "manual_requirements") == 0 || strcmp(key, "instructions") == 0;
}

static int parse_root_object(ParseCtx* ctx, ConfigV1Ast* ast)
{
    if (!enter_container(ctx) || !expect(ctx, JSON_TOK_LBRACE))
        return 0;

    int has_kernel       = 0;
    int has_adapter      = 0;
    int has_instructions = 0;

    const JsonToken* tok = peek(ctx);
    if (tok && tok->type == JSON_TOK_RBRACE)
    {
        set_err(ctx, BS_CONFIG_PARSE_ERR_SCHEMA, tok->line, tok->column);
        return 0;
    }

    while (1)
    {
        const JsonToken* key_tok = peek(ctx);
        char*            key     = parse_string_value(ctx);
        if (!key)
            return 0;
        if (!is_known_root_key(key))
        {
            set_err(ctx, BS_CONFIG_PARSE_ERR_SCHEMA, key_tok ? key_tok->line : 0,
                    key_tok ? key_tok->column : 0);
            (void)skip_json_value(ctx);
            free(key);
            return 0;
        }
        if (!expect(ctx, JSON_TOK_COLON))
        {
            free(key);
            return 0;
        }

        if (strcmp(key, "kernel_version") == 0)
        {
            if (has_kernel)
            {
                set_err(ctx, BS_CONFIG_PARSE_ERR_SCHEMA, key_tok ? key_tok->line : 0,
                        key_tok ? key_tok->column : 0);
                free(key);
                return 0;
            }
            ast->kernel_version = parse_string_value(ctx);
            has_kernel          = ast->kernel_version && ast->kernel_version[0];
        }
        else if (strcmp(key, "adapter_version") == 0)
        {
            if (has_adapter)
            {
                set_err(ctx, BS_CONFIG_PARSE_ERR_SCHEMA, key_tok ? key_tok->line : 0,
                        key_tok ? key_tok->column : 0);
                free(key);
                return 0;
            }
            ast->adapter_version = parse_string_value(ctx);
            has_adapter          = ast->adapter_version && ast->adapter_version[0];
        }
        else if (strcmp(key, "manual_requirements") == 0)
        {
            if (!parse_manual_array(ctx, &ast->manual_requirements,
                                    &ast->manual_requirements_count))
            {
                free(key);
                return 0;
            }
        }
        else if (strcmp(key, "instructions") == 0)
        {
            if (has_instructions)
            {
                set_err(ctx, BS_CONFIG_PARSE_ERR_SCHEMA, key_tok ? key_tok->line : 0,
                        key_tok ? key_tok->column : 0);
                free(key);
                return 0;
            }
            if (!parse_instructions_array(ctx, &ast->instructions, &ast->instructions_count))
            {
                free(key);
                return 0;
            }
            has_instructions = 1;
        }
        free(key);

        tok = peek(ctx);
        if (!tok)
            return 0;
        if (tok->type == JSON_TOK_COMMA)
        {
            advance(ctx);
            continue;
        }
        if (tok->type == JSON_TOK_RBRACE)
        {
            advance(ctx);
            leave_container(ctx);
            break;
        }
        set_err(ctx, BS_CONFIG_PARSE_ERR_PARSE, tok->line, tok->column);
        return 0;
    }

    if (!has_kernel || !has_adapter || !has_instructions)
    {
        set_err(ctx, BS_CONFIG_PARSE_ERR_SCHEMA, 0, 0);
        return 0;
    }
    return 1;
}

BsStatus bs_json_parse_config_v1(const char* data, size_t len, ConfigV1Ast** out_ast,
                                 size_t* error_line, size_t* error_column)
{
    if (!data || !out_ast)
        return bs_status_from_config_parse(BS_CONFIG_PARSE_ERR_INVALID_ARG);

    if (len > BS_JSON_MAX_INPUT_BYTES)
        return bs_status_from_config_parse(BS_CONFIG_PARSE_ERR_PARSE);

    JsonLexer lex;
    bs_json_lexer_init(&lex, data, len);

    ParseCtx ctx = {.lex          = &lex,
                    .error_line   = error_line,
                    .error_column = error_column,
                    .depth        = 0,
                    .err_code     = 0};

    bs_json_lexer_next(&lex);

    ConfigV1Ast* ast = bs_config_v1_ast_create();
    if (!ast)
        return bs_status_from_config_parse(BS_CONFIG_PARSE_ERR_OOM);

    if (!parse_root_object(&ctx, ast))
    {
        bs_config_v1_ast_destroy(ast);
        return fail_status(&ctx);
    }

    const JsonToken* tok = peek(&ctx);
    if (!tok || tok->type != JSON_TOK_EOF)
    {
        set_err(&ctx, BS_CONFIG_PARSE_ERR_PARSE, tok ? tok->line : 0, tok ? tok->column : 0);
        bs_config_v1_ast_destroy(ast);
        return fail_status(&ctx);
    }

    *out_ast = ast;
    return BS_STATUS_OK;
}
