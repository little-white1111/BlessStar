#include "bs/adapter/parser/FormatAdapter.h"

#include <stdlib.h>
#include <string.h>

FormatAdapter* format_adapter_create(const char* name, FormatParseFunc parse,
                                     FormatSerializeFunc serialize)
{
    if (!name)
        return NULL;

    FormatAdapter* adapter = (FormatAdapter*)malloc(sizeof(FormatAdapter));
    if (!adapter)
        return NULL;

    adapter->name            = strdup(name);
    adapter->description     = NULL;
    adapter->extensions      = NULL;
    adapter->extension_count = 0;
    adapter->parse           = parse;
    adapter->serialize       = serialize;
    adapter->cleanup         = NULL;
    adapter->next            = NULL;

    return adapter;
}

void format_adapter_destroy(FormatAdapter* adapter)
{
    if (!adapter)
        return;

    if (adapter->name)
        free((void*)adapter->name);
    if (adapter->description)
        free((void*)adapter->description);
    if (adapter->extensions)
    {
        for (size_t i = 0; i < adapter->extension_count; i++)
        {
            free((void*)adapter->extensions[i]);
        }
        free(adapter->extensions);
    }

    free(adapter);
}

int format_adapter_parse(FormatAdapter* adapter, const char* source, size_t length,
                         ParsedData** output)
{
    if (!adapter || !adapter->parse || !source || !output)
        return -1;
    return adapter->parse(source, length, output);
}

int format_adapter_serialize(FormatAdapter* adapter, const ParsedData* data, char** output,
                             size_t* length)
{
    if (!adapter || !adapter->serialize || !data || !output || !length)
        return -1;
    return adapter->serialize(data, output, length);
}

void format_adapter_cleanup(FormatAdapter* adapter, ParsedData* data)
{
    if (!adapter || !adapter->cleanup || !data)
        return;
    adapter->cleanup(data);
}

const char* format_adapter_get_name(const FormatAdapter* adapter)
{
    return adapter ? adapter->name : NULL;
}

int format_adapter_supports_extension(const FormatAdapter* adapter, const char* extension)
{
    if (!adapter || !extension)
        return 0;

    for (size_t i = 0; i < adapter->extension_count; i++)
    {
        if (strcmp(adapter->extensions[i], extension) == 0)
        {
            return 1;
        }
    }

    return 0;
}
