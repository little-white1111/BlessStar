#ifndef BS_ADAPTER_PARSER_JSON_LEXER_H
#define BS_ADAPTER_PARSER_JSON_LEXER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum JsonTokenType
    {
        JSON_TOK_EOF = 0,
        JSON_TOK_LBRACE,
        JSON_TOK_RBRACE,
        JSON_TOK_LBRACKET,
        JSON_TOK_RBRACKET,
        JSON_TOK_COLON,
        JSON_TOK_COMMA,
        JSON_TOK_STRING,
        JSON_TOK_NUMBER,
        JSON_TOK_TRUE,
        JSON_TOK_FALSE,
        JSON_TOK_NULL,
        JSON_TOK_ERROR
    } JsonTokenType;

    typedef struct JsonToken
    {
        JsonTokenType type;
        const char*   start;
        size_t        length;
        size_t        line;
        size_t        column;
    } JsonToken;

    typedef struct JsonLexer
    {
        const char* start;
        const char* cur;
        const char* end;
        size_t      line;
        size_t      column;
        JsonToken   current;
        int         has_error;
    } JsonLexer;

#define BS_JSON_MAX_INPUT_BYTES (1024u * 1024u)
#define BS_JSON_MAX_DEPTH 32u
/** Legacy codepoint-oriented cap (lexer now enforces bytes; see STRING_BYTES). */
#define BS_JSON_MAX_STRING_LEN 4096u
/** Decoded UTF-8 byte budget per string token (AUD-IX-4 · day13). */
#define BS_JSON_MAX_STRING_BYTES (16384u)
/** Max instructions[] entries in Config JSON v1 (AUD-IX-4 · closes BOUND-G-02). */
#define BS_JSON_MAX_INSTRUCTIONS (2048u)
/** Max manual_requirements[] string entries (AUD-IX-4). */
#define BS_JSON_MAX_MANUAL_ITEMS (256u)

    void json_lexer_init(JsonLexer* lex, const char* data, size_t len);

    /** Advance and return previous token (or ERROR on failure). */
    JsonToken json_lexer_next(JsonLexer* lex);

    const JsonToken* json_lexer_current(const JsonLexer* lex);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PARSER_JSON_LEXER_H */
