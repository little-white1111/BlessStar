#include "bs/kernel/ir/Metadata.h"

#include <stdlib.h>
#include <string.h>

static MetadataEntry* metadata_entry_find(MetadataEntry* head, const char* key)
{
    while (head)
    {
        if (head->key && key && strcmp(head->key, key) == 0)
            return head;
        head = head->next;
    }
    return NULL;
}

static void metadata_entry_free_chain(MetadataEntry* head)
{
    while (head)
    {
        MetadataEntry* next = head->next;
        free((void*)head->key);
        if (head->value.type == METADATA_TYPE_STRING && head->value.value.string_val)
            free((void*)head->value.value.string_val);
        if (head->value.type == METADATA_TYPE_BINARY && head->value.value.binary_val.data)
            free((void*)head->value.value.binary_val.data);
        free(head);
        head = next;
    }
}

Metadata* bs_metadata_create(void)
{
    Metadata* meta = (Metadata*)calloc(1, sizeof(Metadata));
    return meta;
}

void bs_metadata_destroy(Metadata* meta)
{
    if (!meta)
        return;
    metadata_entry_free_chain(meta->head);
    free(meta);
}

static int metadata_insert(Metadata* meta, const char* key, MetadataValue value)
{
    if (!meta || !key)
        return -1;

    MetadataEntry* existing = metadata_entry_find(meta->head, key);
    if (existing)
    {
        if (existing->value.type == METADATA_TYPE_STRING && existing->value.value.string_val)
            free((void*)existing->value.value.string_val);
        if (existing->value.type == METADATA_TYPE_BINARY && existing->value.value.binary_val.data)
            free((void*)existing->value.value.binary_val.data);
        existing->value = value;
        return 0;
    }

    MetadataEntry* entry = (MetadataEntry*)calloc(1, sizeof(MetadataEntry));
    if (!entry)
        return -1;

    entry->key   = strdup(key);
    entry->value = value;
    entry->next  = meta->head;
    meta->head   = entry;
    meta->count++;
    return 0;
}

int bs_metadata_set_string(Metadata* meta, const char* key, const char* value)
{
    MetadataValue v;
    v.type             = METADATA_TYPE_STRING;
    v.value.string_val = value ? strdup(value) : strdup("");
    if (!v.value.string_val)
        return -1;
    return metadata_insert(meta, key, v);
}

int bs_metadata_set_integer(Metadata* meta, const char* key, int64_t value)
{
    MetadataValue v;
    v.type              = METADATA_TYPE_INTEGER;
    v.value.integer_val = value;
    return metadata_insert(meta, key, v);
}

int bs_metadata_set_double(Metadata* meta, const char* key, double value)
{
    MetadataValue v;
    v.type             = METADATA_TYPE_DOUBLE;
    v.value.double_val = value;
    return metadata_insert(meta, key, v);
}

int bs_metadata_set_boolean(Metadata* meta, const char* key, int value)
{
    MetadataValue v;
    v.type              = METADATA_TYPE_BOOLEAN;
    v.value.boolean_val = value ? 1 : 0;
    return metadata_insert(meta, key, v);
}

int bs_metadata_set_binary(Metadata* meta, const char* key, const uint8_t* data, size_t length)
{
    MetadataValue v;
    v.type = METADATA_TYPE_BINARY;
    if (length > 0 && data)
    {
        uint8_t* copy = (uint8_t*)malloc(length);
        if (!copy)
            return -1;
        memcpy(copy, data, length);
        v.value.binary_val.data   = copy;
        v.value.binary_val.length = length;
    }
    else
    {
        v.value.binary_val.data   = NULL;
        v.value.binary_val.length = 0;
    }
    return metadata_insert(meta, key, v);
}

int bs_metadata_get_string(const Metadata* meta, const char* key, const char** value)
{
    if (!meta || !key || !value)
        return -1;
    MetadataEntry* entry = metadata_entry_find(meta->head, key);
    if (!entry || entry->value.type != METADATA_TYPE_STRING)
        return -1;
    *value = entry->value.value.string_val;
    return 0;
}

int bs_metadata_get_integer(const Metadata* meta, const char* key, int64_t* value)
{
    if (!meta || !key || !value)
        return -1;
    MetadataEntry* entry = metadata_entry_find(meta->head, key);
    if (!entry || entry->value.type != METADATA_TYPE_INTEGER)
        return -1;
    *value = entry->value.value.integer_val;
    return 0;
}

int bs_metadata_get_double(const Metadata* meta, const char* key, double* value)
{
    if (!meta || !key || !value)
        return -1;
    MetadataEntry* entry = metadata_entry_find(meta->head, key);
    if (!entry || entry->value.type != METADATA_TYPE_DOUBLE)
        return -1;
    *value = entry->value.value.double_val;
    return 0;
}

int bs_metadata_get_boolean(const Metadata* meta, const char* key, int* value)
{
    if (!meta || !key || !value)
        return -1;
    MetadataEntry* entry = metadata_entry_find(meta->head, key);
    if (!entry || entry->value.type != METADATA_TYPE_BOOLEAN)
        return -1;
    *value = entry->value.value.boolean_val;
    return 0;
}

int bs_metadata_get_binary(const Metadata* meta, const char* key, const uint8_t** data,
                           size_t* length)
{
    if (!meta || !key || !data || !length)
        return -1;
    MetadataEntry* entry = metadata_entry_find(meta->head, key);
    if (!entry || entry->value.type != METADATA_TYPE_BINARY)
        return -1;
    *data   = entry->value.value.binary_val.data;
    *length = entry->value.value.binary_val.length;
    return 0;
}

int bs_metadata_has_key(const Metadata* meta, const char* key)
{
    return meta && key && metadata_entry_find(meta->head, key) != NULL;
}

void bs_metadata_remove(Metadata* meta, const char* key)
{
    if (!meta || !key)
        return;

    MetadataEntry* prev = NULL;
    MetadataEntry* cur  = meta->head;
    while (cur)
    {
        if (cur->key && strcmp(cur->key, key) == 0)
        {
            if (prev)
                prev->next = cur->next;
            else
                meta->head = cur->next;
            MetadataEntry* dead = cur;
            cur                 = cur->next;
            free((void*)dead->key);
            if (dead->value.type == METADATA_TYPE_STRING && dead->value.value.string_val)
                free((void*)dead->value.value.string_val);
            if (dead->value.type == METADATA_TYPE_BINARY && dead->value.value.binary_val.data)
                free((void*)dead->value.value.binary_val.data);
            free(dead);
            if (meta->count > 0)
                meta->count--;
            return;
        }
        prev = cur;
        cur  = cur->next;
    }
}

size_t bs_metadata_count(const Metadata* meta)
{
    return meta ? meta->count : 0;
}

void bs_metadata_clear(Metadata* meta)
{
    if (!meta)
        return;
    metadata_entry_free_chain(meta->head);
    meta->head  = NULL;
    meta->count = 0;
}

Metadata* bs_metadata_clone(const Metadata* meta)
{
    if (!meta)
        return NULL;
    Metadata* copy = bs_metadata_create();
    if (!copy)
        return NULL;

    for (MetadataEntry* e = meta->head; e; e = e->next)
    {
        if (!e->key)
            continue;
        switch (e->value.type)
        {
        case METADATA_TYPE_STRING:
            if (bs_metadata_set_string(copy, e->key, e->value.value.string_val) != 0)
                goto fail;
            break;
        case METADATA_TYPE_INTEGER:
            if (bs_metadata_set_integer(copy, e->key, e->value.value.integer_val) != 0)
                goto fail;
            break;
        case METADATA_TYPE_DOUBLE:
            if (bs_metadata_set_double(copy, e->key, e->value.value.double_val) != 0)
                goto fail;
            break;
        case METADATA_TYPE_BOOLEAN:
            if (bs_metadata_set_boolean(copy, e->key, e->value.value.boolean_val) != 0)
                goto fail;
            break;
        case METADATA_TYPE_BINARY:
            if (bs_metadata_set_binary(copy, e->key, e->value.value.binary_val.data,
                                       e->value.value.binary_val.length) != 0)
                goto fail;
            break;
        default:
            break;
        }
    }
    return copy;

fail:
    bs_metadata_destroy(copy);
    return NULL;
}

int bs_metadata_merge(Metadata* dest, const Metadata* src)
{
    if (!dest || !src)
        return -1;
    Metadata* copy = bs_metadata_clone(src);
    if (!copy)
        return -1;

    for (MetadataEntry* e = copy->head; e; e = e->next)
    {
        if (!e->key)
            continue;
        switch (e->value.type)
        {
        case METADATA_TYPE_STRING:
            bs_metadata_set_string(dest, e->key, e->value.value.string_val);
            break;
        case METADATA_TYPE_INTEGER:
            bs_metadata_set_integer(dest, e->key, e->value.value.integer_val);
            break;
        case METADATA_TYPE_DOUBLE:
            bs_metadata_set_double(dest, e->key, e->value.value.double_val);
            break;
        case METADATA_TYPE_BOOLEAN:
            bs_metadata_set_boolean(dest, e->key, e->value.value.boolean_val);
            break;
        case METADATA_TYPE_BINARY:
            bs_metadata_set_binary(dest, e->key, e->value.value.binary_val.data,
                                   e->value.value.binary_val.length);
            break;
        default:
            break;
        }
    }
    bs_metadata_destroy(copy);
    return 0;
}
