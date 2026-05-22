#include "bs/adapter/parser/SchemaRegistry.h"

#include <stdlib.h>
#include <string.h>

typedef struct SchemaEntry
{
    const char*         name;
    const char*         schema;
    struct SchemaEntry* next;
} SchemaEntry;

struct SchemaRegistry
{
    SchemaEntry* head;
    size_t       count;
};

SchemaRegistry* schema_registry_create(void)
{
    SchemaRegistry* registry = (SchemaRegistry*)malloc(sizeof(SchemaRegistry));
    if (!registry)
        return NULL;

    registry->head  = NULL;
    registry->count = 0;

    return registry;
}

void schema_registry_destroy(SchemaRegistry* registry)
{
    if (!registry)
        return;

    SchemaEntry* entry = registry->head;
    while (entry)
    {
        SchemaEntry* next = entry->next;
        if (entry->name)
            free((void*)entry->name);
        if (entry->schema)
            free((void*)entry->schema);
        free(entry);
        entry = next;
    }

    free(registry);
}

int schema_registry_register(SchemaRegistry* registry, const char* name, const char* schema)
{
    if (!registry || !name || !schema)
        return -1;

    // Check if already exists
    SchemaEntry* existing = registry->head;
    while (existing)
    {
        if (strcmp(existing->name, name) == 0)
        {
            return -1;
        }
        existing = existing->next;
    }

    SchemaEntry* entry = (SchemaEntry*)malloc(sizeof(SchemaEntry));
    if (!entry)
        return -1;

    entry->name    = strdup(name);
    entry->schema  = strdup(schema);
    entry->next    = registry->head;
    registry->head = entry;
    registry->count++;

    return 0;
}

int schema_registry_unregister(SchemaRegistry* registry, const char* name)
{
    if (!registry || !name)
        return -1;

    SchemaEntry* prev    = NULL;
    SchemaEntry* current = registry->head;

    while (current)
    {
        if (strcmp(current->name, name) == 0)
        {
            if (prev)
            {
                prev->next = current->next;
            }
            else
            {
                registry->head = current->next;
            }

            if (current->name)
                free((void*)current->name);
            if (current->schema)
                free((void*)current->schema);
            free(current);
            registry->count--;
            return 0;
        }

        prev    = current;
        current = current->next;
    }

    return -1;
}

const char* schema_registry_get(SchemaRegistry* registry, const char* name)
{
    if (!registry || !name)
        return NULL;

    SchemaEntry* entry = registry->head;
    while (entry)
    {
        if (strcmp(entry->name, name) == 0)
        {
            return entry->schema;
        }
        entry = entry->next;
    }

    return NULL;
}

int schema_registry_validate(SchemaRegistry* registry, const char* name, const char* data)
{
    if (!registry || !name || !data)
        return -1;

    const char* schema = schema_registry_get(registry, name);
    if (!schema)
        return -1;

    (void)data;
    (void)schema;
    return 0;
}

int schema_registry_validate_data(SchemaRegistry* registry, const char* schema, const char* data)
{
    if (!registry || !schema || !data)
        return -1;

    (void)schema;
    (void)data;
    return 0;
}

int schema_registry_has_schema(SchemaRegistry* registry, const char* name)
{
    if (!registry || !name)
        return 0;

    SchemaEntry* entry = registry->head;
    while (entry)
    {
        if (strcmp(entry->name, name) == 0)
        {
            return 1;
        }
        entry = entry->next;
    }

    return 0;
}

size_t schema_registry_get_count(const SchemaRegistry* registry)
{
    return registry ? registry->count : 0;
}

int schema_registry_list_schemas(SchemaRegistry* registry, const char*** names, size_t* count)
{
    if (!registry || !names || !count)
        return -1;

    if (registry->count == 0)
    {
        *names = NULL;
        *count = 0;
        return 0;
    }

    *names = (const char**)malloc(sizeof(char*) * registry->count);
    if (!*names)
        return -1;

    size_t       i     = 0;
    SchemaEntry* entry = registry->head;
    while (entry)
    {
        (*names)[i++] = entry->name;
        entry         = entry->next;
    }

    *count = registry->count;
    return 0;
}

void schema_registry_free_names(const char*** names, size_t count)
{
    if (!names || !*names)
        return;
    free(*names);
    *names = NULL;
}
