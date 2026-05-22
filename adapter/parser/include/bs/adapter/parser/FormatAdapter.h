#ifndef BS_ADAPTER_PARSER_FORMAT_ADAPTER_H
#define BS_ADAPTER_PARSER_FORMAT_ADAPTER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct FormatAdapter FormatAdapter;
    typedef struct ParsedData    ParsedData;

    struct ParsedData
    {
        const char* type;
        void*       data;
        size_t      size;
        ParsedData* children;
        size_t      child_count;
    };

    typedef int (*FormatParseFunc)(const char* source, size_t length, ParsedData** output);
    typedef int (*FormatSerializeFunc)(const ParsedData* data, char** output, size_t* length);
    typedef void (*FormatCleanupFunc)(ParsedData* data);

    struct FormatAdapter
    {
        const char*         name;
        const char*         description;
        const char**        extensions;
        size_t              extension_count;
        FormatParseFunc     parse;
        FormatSerializeFunc serialize;
        FormatCleanupFunc   cleanup;
        FormatAdapter*      next;
    };

    FormatAdapter* format_adapter_create(const char* name, FormatParseFunc parse,
                                         FormatSerializeFunc serialize);
    void           format_adapter_destroy(FormatAdapter* adapter);

    int  format_adapter_parse(FormatAdapter* adapter, const char* source, size_t length,
                              ParsedData** output);
    int  format_adapter_serialize(FormatAdapter* adapter, const ParsedData* data, char** output,
                                  size_t* length);
    void format_adapter_cleanup(FormatAdapter* adapter, ParsedData* data);

    const char* format_adapter_get_name(const FormatAdapter* adapter);
    int format_adapter_supports_extension(const FormatAdapter* adapter, const char* extension);

#ifdef __cplusplus
}
#endif

#endif // BS_ADAPTER_PARSER_FORMAT_ADAPTER_H
