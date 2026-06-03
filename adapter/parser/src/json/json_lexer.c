#include "bs/adapter/parser/json_lexer.h"
#include "bs/adapter/parser/json_utf8.h"

#include <ctype.h>

#include <string.h>

static int is_hex(int c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int hex_val(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return 10 + (c - 'a');
    return 10 + (c - 'A');
}

static void set_error_token(JsonLexer* lex, size_t line, size_t column)
{
    lex->has_error      = 1;
    lex->current.type   = JSON_TOK_ERROR;
    lex->current.start  = lex->cur;
    lex->current.length = 0;
    lex->current.line   = line;
    lex->current.column = column;
}

static void skip_ws(JsonLexer* lex)
{
    while (lex->cur < lex->end)
    {
        const char c = *lex->cur;
        if (c == ' ' || c == '\t' || c == '\r')
        {
            ++lex->cur;
            ++lex->column;
            continue;
        }
        if (c == '\n')
        {
            ++lex->cur;
            ++lex->line;
            lex->column = 1;
            continue;
        }
        break;
    }
}

static JsonToken make_token(JsonLexer* lex, JsonTokenType type, const char* start, size_t len,
                            size_t line, size_t column)
{
    (void)lex;
    JsonToken tok;
    tok.type   = type;
    tok.start  = start;
    tok.length = len;
    tok.line   = line;
    tok.column = column;
    return tok;
}

static JsonToken lex_string(JsonLexer* lex)
{
    const size_t line   = lex->line;
    const size_t column = lex->column;
    const char*  start  = lex->cur;
    ++lex->cur;
    ++lex->column;

    size_t decoded_bytes      = 0;
    size_t decoded_codepoints = 0;
    while (lex->cur < lex->end)
    {
        const char c = *lex->cur;
        if (c == '"')
        {
            ++lex->cur;
            ++lex->column;
            if (decoded_bytes > BS_JSON_MAX_STRING_BYTES ||
                decoded_codepoints > BS_JSON_MAX_STRING_LEN)
            {
                set_error_token(lex, line, column);
                return lex->current;
            }
            return make_token(lex, JSON_TOK_STRING, start, (size_t)(lex->cur - start), line,
                              column);
        }
        if (c == '\\')
        {
            ++lex->cur;
            ++lex->column;
            if (lex->cur >= lex->end)
            {
                set_error_token(lex, line, column);
                return lex->current;
            }
            const char e = *lex->cur;
            if (e == 'u')
            {
                ++lex->cur;
                ++lex->column;
                unsigned int cp = 0;
                for (int i = 0; i < 4; ++i)
                {
                    if (lex->cur >= lex->end || !is_hex((unsigned char)*lex->cur))
                    {
                        set_error_token(lex, line, column);
                        return lex->current;
                    }
                    cp = (cp << 4) + (unsigned)hex_val(*lex->cur);
                    ++lex->cur;
                    ++lex->column;
                }
                if (!bs_json_utf8_codepoint_valid(cp))
                {
                    set_error_token(lex, line, column);
                    return lex->current;
                }
                if (decoded_bytes + 4 > BS_JSON_MAX_STRING_BYTES ||
                    decoded_codepoints + 1 > BS_JSON_MAX_STRING_LEN)
                {
                    set_error_token(lex, line, column);
                    return lex->current;
                }
                decoded_bytes += 4;
                decoded_codepoints += 1;
                continue;
            }
            if (e == '"' || e == '\\' || e == '/' || e == 'b' || e == 'f' || e == 'n' || e == 'r' ||
                e == 't')
            {
                ++lex->cur;
                ++lex->column;
                if (decoded_bytes + 1 > BS_JSON_MAX_STRING_BYTES ||
                    decoded_codepoints + 1 > BS_JSON_MAX_STRING_LEN)
                {
                    set_error_token(lex, line, column);
                    return lex->current;
                }
                decoded_bytes += 1;
                decoded_codepoints += 1;
                continue;
            }
            set_error_token(lex, line, column);
            return lex->current;
        }
        if ((unsigned char)c < 0x20u)
        {
            set_error_token(lex, line, column);
            return lex->current;
        }
        if ((unsigned char)c >= 0x80u)
        {
            const char*  p = lex->cur;
            unsigned int cp;
            if (!bs_json_utf8_decode_advance(&p, lex->end, &cp))
            {
                set_error_token(lex, line, column);
                return lex->current;
            }
            const size_t adv = (size_t)(p - lex->cur);
            lex->column += adv;
            lex->cur = p;
            if (decoded_bytes + adv > BS_JSON_MAX_STRING_BYTES ||
                decoded_codepoints + 1 > BS_JSON_MAX_STRING_LEN)
            {
                set_error_token(lex, line, column);
                return lex->current;
            }
            decoded_bytes += adv;
            decoded_codepoints += 1;
        }
        else
        {
            ++lex->cur;
            ++lex->column;
            if (decoded_bytes + 1 > BS_JSON_MAX_STRING_BYTES ||
                decoded_codepoints + 1 > BS_JSON_MAX_STRING_LEN)
            {
                set_error_token(lex, line, column);
                return lex->current;
            }
            decoded_bytes += 1;
            decoded_codepoints += 1;
        }
    }
    set_error_token(lex, line, column);
    return lex->current;
}

static JsonToken lex_number(JsonLexer* lex)
{
    const size_t line   = lex->line;
    const size_t column = lex->column;
    const char*  start  = lex->cur;

    if (*lex->cur == '-')
    {
        ++lex->cur;
        ++lex->column;
    }
    if (lex->cur >= lex->end || !isdigit((unsigned char)*lex->cur))
    {
        set_error_token(lex, line, column);
        return lex->current;
    }
    while (lex->cur < lex->end && isdigit((unsigned char)*lex->cur))
    {
        ++lex->cur;
        ++lex->column;
    }
    if (lex->cur < lex->end && *lex->cur == '.')
    {
        set_error_token(lex, line, column);
        return lex->current;
    }
    if (lex->cur < lex->end && (*lex->cur == 'e' || *lex->cur == 'E'))
    {
        set_error_token(lex, line, column);
        return lex->current;
    }
    return make_token(lex, JSON_TOK_NUMBER, start, (size_t)(lex->cur - start), line, column);
}

static int match_literal(JsonLexer* lex, const char* lit)
{
    const size_t n = strlen(lit);
    if ((size_t)(lex->end - lex->cur) < n)
        return 0;
    if (strncmp(lex->cur, lit, n) != 0)
        return 0;
    if (lex->cur + n < lex->end)
    {
        const char next = lex->cur[n];
        if (isalnum((unsigned char)next) || next == '_')
            return 0;
    }
    lex->cur += n;
    lex->column += n;
    return 1;
}

void bs_json_lexer_init(JsonLexer* lex, const char* data, size_t len)
{
    if (!lex)
        return;
    lex->start     = data;
    lex->cur       = data;
    lex->end       = data + len;
    lex->line      = 1;
    lex->column    = 1;
    lex->has_error = 0;
    lex->current   = make_token(lex, JSON_TOK_EOF, data, 0, 1, 1);
}

JsonToken bs_json_lexer_next(JsonLexer* lex)
{
    if (!lex || lex->has_error)
        return lex ? lex->current : (JsonToken){JSON_TOK_ERROR, NULL, 0, 0, 0};

    skip_ws(lex);
    const size_t line   = lex->line;
    const size_t column = lex->column;

    if (lex->cur >= lex->end)
    {
        lex->current = make_token(lex, JSON_TOK_EOF, lex->cur, 0, line, column);
        return lex->current;
    }

    const char  c     = *lex->cur;
    const char* start = lex->cur;
    ++lex->cur;
    ++lex->column;

    switch (c)
    {
    case '{':
        lex->current = make_token(lex, JSON_TOK_LBRACE, start, 1, line, column);
        break;
    case '}':
        lex->current = make_token(lex, JSON_TOK_RBRACE, start, 1, line, column);
        break;
    case '[':
        lex->current = make_token(lex, JSON_TOK_LBRACKET, start, 1, line, column);
        break;
    case ']':
        lex->current = make_token(lex, JSON_TOK_RBRACKET, start, 1, line, column);
        break;
    case ':':
        lex->current = make_token(lex, JSON_TOK_COLON, start, 1, line, column);
        break;
    case ',':
        lex->current = make_token(lex, JSON_TOK_COMMA, start, 1, line, column);
        break;
    case '"':
        --lex->cur;
        --lex->column;
        lex->current = lex_string(lex);
        break;
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        --lex->cur;
        --lex->column;
        lex->current = lex_number(lex);
        break;
    default:
        --lex->cur;
        --lex->column;
        if (match_literal(lex, "true"))
            lex->current = make_token(lex, JSON_TOK_TRUE, start, 4, line, column);
        else if (match_literal(lex, "false"))
            lex->current = make_token(lex, JSON_TOK_FALSE, start, 5, line, column);
        else if (match_literal(lex, "null"))
            lex->current = make_token(lex, JSON_TOK_NULL, start, 4, line, column);
        else
            set_error_token(lex, line, column);
        break;
    }
    return lex->current;
}

const JsonToken* bs_json_lexer_current(const JsonLexer* lex)
{
    return lex ? &lex->current : NULL;
}
