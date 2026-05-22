#ifndef BS_ADAPTER_PARSER_PARSER_H
#define BS_ADAPTER_PARSER_PARSER_H

/* Legacy skeleton: MVP config reload uses bs_config_parse_bytes (config_parse.h). */

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct IRInstruction IRInstruction;
    typedef struct FormatAdapter FormatAdapter;
    typedef struct Parser        Parser;

    Parser* parser_create(void);
    void    parser_destroy(Parser* parser);

    int            parser_register_format(Parser* parser, FormatAdapter* adapter);
    int            parser_unregister_format(Parser* parser, const char* format_name);
    FormatAdapter* parser_get_format(Parser* parser, const char* format_name);

    int parser_register_schema(Parser* parser, const char* name, const char* schema_json);
    int parser_unregister_schema(Parser* parser, const char* name);
    int parser_set_default_schema(Parser* parser, const char* schema_name);

    IRInstruction* parser_parse(Parser* parser, const char* source, const char* format);
    IRInstruction* parser_parse_with_schema(Parser* parser, const char* source, const char* format,
                                            const char* schema_name);

    int parser_enable_metaprogramming(Parser* parser, int enable);
    int parser_set_sandbox_mode(Parser* parser, int enable);

    const char* parser_get_last_error(const Parser* parser);
    void        parser_clear_error(Parser* parser);

#ifdef __cplusplus
}
#endif

#endif // BS_ADAPTER_PARSER_PARSER_H
