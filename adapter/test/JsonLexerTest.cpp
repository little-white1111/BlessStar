#include "bs/adapter/parser/json_lexer.h"

#include <cassert>
#include <cstring>

int main()
{
    const char* json = "{\"a\":1}";
    JsonLexer   lex;
    bs_json_lexer_init(&lex, json, strlen(json));
    bs_json_lexer_next(&lex);
    assert(bs_json_lexer_current(&lex)->type == JSON_TOK_LBRACE);
    bs_json_lexer_next(&lex);
    assert(bs_json_lexer_current(&lex)->type == JSON_TOK_STRING);
    bs_json_lexer_next(&lex);
    assert(bs_json_lexer_current(&lex)->type == JSON_TOK_COLON);
    bs_json_lexer_next(&lex);
    assert(bs_json_lexer_current(&lex)->type == JSON_TOK_NUMBER);
    bs_json_lexer_next(&lex);
    assert(bs_json_lexer_current(&lex)->type == JSON_TOK_RBRACE);
    bs_json_lexer_next(&lex);
    assert(bs_json_lexer_current(&lex)->type == JSON_TOK_EOF);

    const char* bad = "{\"x\":1.5}";
    bs_json_lexer_init(&lex, bad, strlen(bad));
    bs_json_lexer_next(&lex);
    while (bs_json_lexer_current(&lex)->type != JSON_TOK_EOF &&
           bs_json_lexer_current(&lex)->type != JSON_TOK_ERROR)
        bs_json_lexer_next(&lex);
    assert(bs_json_lexer_current(&lex)->type == JSON_TOK_ERROR);
    return 0;
}
