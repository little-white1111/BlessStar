#include "bs/adapter/parser/json_lexer.h"

#include <cassert>
#include <cstring>

int main()
{
    const char* json = "{\"a\":1}";
    JsonLexer   lex;
    json_lexer_init(&lex, json, strlen(json));
    json_lexer_next(&lex);
    assert(json_lexer_current(&lex)->type == JSON_TOK_LBRACE);
    json_lexer_next(&lex);
    assert(json_lexer_current(&lex)->type == JSON_TOK_STRING);
    json_lexer_next(&lex);
    assert(json_lexer_current(&lex)->type == JSON_TOK_COLON);
    json_lexer_next(&lex);
    assert(json_lexer_current(&lex)->type == JSON_TOK_NUMBER);
    json_lexer_next(&lex);
    assert(json_lexer_current(&lex)->type == JSON_TOK_RBRACE);
    json_lexer_next(&lex);
    assert(json_lexer_current(&lex)->type == JSON_TOK_EOF);

    const char* bad = "{\"x\":1.5}";
    json_lexer_init(&lex, bad, strlen(bad));
    json_lexer_next(&lex);
    while (json_lexer_current(&lex)->type != JSON_TOK_EOF &&
           json_lexer_current(&lex)->type != JSON_TOK_ERROR)
        json_lexer_next(&lex);
    assert(json_lexer_current(&lex)->type == JSON_TOK_ERROR);
    return 0;
}
