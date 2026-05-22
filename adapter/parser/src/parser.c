#include "bs/adapter/parser/Parser.h"

#include <stdlib.h>
#include <string.h>

struct Parser
{
    FormatAdapter* formats;
    size_t         format_count;
    void*          schema_registry;
    void*          meta_executor;
    int            metaprogramming_enabled;
    int            sandbox_mode;
    char*          last_error;
};

Parser* parser_create(void)
{
    Parser* parser = (Parser*)malloc(sizeof(Parser));
    if (!parser)
        return NULL;

    parser->formats                 = NULL;
    parser->format_count            = 0;
    parser->schema_registry         = NULL;
    parser->meta_executor           = NULL;
    parser->metaprogramming_enabled = 0;
    parser->sandbox_mode            = 1;
    parser->last_error              = NULL;

    return parser;
}

void parser_destroy(Parser* parser)
{
    if (!parser)
        return;

    if (parser->formats)
    {
        for (size_t i = 0; i < parser->format_count; i++)
        {
            format_adapter_destroy(&parser->formats[i]);
        }
        free(parser->formats);
    }

    if (parser->schema_registry)
    {
        // schema_registry_destroy(parser->schema_registry);
    }

    if (parser->meta_executor)
    {
        // meta_executor_destroy(parser->meta_executor);
    }

    if (parser->last_error)
    {
        free(parser->last_error);
    }

    free(parser);
}

int parser_register_format(Parser* parser, FormatAdapter* adapter)
{
    if (!parser || !adapter)
        return -1;

    FormatAdapter* new_formats = (FormatAdapter*)realloc(
        parser->formats, sizeof(FormatAdapter) * (parser->format_count + 1));
    if (!new_formats)
        return -1;

    parser->formats                       = new_formats;
    parser->formats[parser->format_count] = *adapter;
    parser->format_count++;

    return 0;
}

int parser_unregister_format(Parser* parser, const char* format_name)
{
    if (!parser || !format_name)
        return -1;

    for (size_t i = 0; i < parser->format_count; i++)
    {
        if (strcmp(format_adapter_get_name(&parser->formats[i]), format_name) == 0)
        {
            format_adapter_destroy(&parser->formats[i]);

            for (size_t j = i; j < parser->format_count - 1; j++)
            {
                parser->formats[j] = parser->formats[j + 1];
            }

            parser->format_count--;
            return 0;
        }
    }

    return -1;
}

FormatAdapter* parser_get_format(Parser* parser, const char* format_name)
{
    if (!parser || !format_name)
        return NULL;

    for (size_t i = 0; i < parser->format_count; i++)
    {
        if (strcmp(format_adapter_get_name(&parser->formats[i]), format_name) == 0)
        {
            return &parser->formats[i];
        }
    }

    return NULL;
}

int parser_register_schema(Parser* parser, const char* name, const char* schema_json)
{
    if (!parser || !name || !schema_json)
        return -1;

    if (!parser->schema_registry)
    {
        // parser->schema_registry = schema_registry_create();
    }

    // return schema_registry_register(parser->schema_registry, name, schema_json);
    (void)schema_json;
    return 0;
}

int parser_unregister_schema(Parser* parser, const char* name)
{
    if (!parser || !name)
        return -1;
    if (!parser->schema_registry)
        return -1;

    // return schema_registry_unregister(parser->schema_registry, name);
    return 0;
}

int parser_set_default_schema(Parser* parser, const char* schema_name)
{
    if (!parser || !schema_name)
        return -1;
    (void)schema_name;
    return 0;
}

IRInstruction* parser_parse(Parser* parser, const char* source, const char* format)
{
    if (!parser || !source || !format)
        return NULL;

    FormatAdapter* adapter = parser_get_format(parser, format);
    if (!adapter)
    {
        parser->last_error = strdup("Format not found");
        return NULL;
    }

    (void)adapter;
    (void)source;

    return NULL;
}

IRInstruction* parser_parse_with_schema(Parser* parser, const char* source, const char* format,
                                        const char* schema_name)
{
    if (!parser || !source || !format)
        return NULL;

    (void)schema_name;
    return parser_parse(parser, source, format);
}

int parser_enable_metaprogramming(Parser* parser, int enable)
{
    if (!parser)
        return -1;
    parser->metaprogramming_enabled = enable;
    return 0;
}

int parser_set_sandbox_mode(Parser* parser, int enable)
{
    if (!parser)
        return -1;
    parser->sandbox_mode = enable;
    return 0;
}

const char* parser_get_last_error(const Parser* parser)
{
    return parser ? parser->last_error : NULL;
}

void parser_clear_error(Parser* parser)
{
    if (!parser)
        return;
    if (parser->last_error)
    {
        free(parser->last_error);
        parser->last_error = NULL;
    }
}
