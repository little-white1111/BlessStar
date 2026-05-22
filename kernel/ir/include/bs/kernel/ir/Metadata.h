#ifndef BS_KERNEL_IR_METADATA_H
#define BS_KERNEL_IR_METADATA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum MetadataType
    {
        METADATA_TYPE_STRING,
        METADATA_TYPE_INTEGER,
        METADATA_TYPE_DOUBLE,
        METADATA_TYPE_BOOLEAN,
        METADATA_TYPE_BINARY,
        METADATA_TYPE_LIST,
        METADATA_TYPE_MAP
    } MetadataType;

    typedef struct MetadataValue MetadataValue;
    typedef struct MetadataEntry MetadataEntry;
    typedef struct Metadata      Metadata;

    struct MetadataValue
    {
        MetadataType type;
        union
        {
            const char* string_val;
            int64_t     integer_val;
            double      double_val;
            int         boolean_val;
            struct
            {
                const uint8_t* data;
                size_t         length;
            } binary_val;
            struct MetadataEntry* list_val;
            struct MetadataEntry* map_val;
        } value;
    };

    struct MetadataEntry
    {
        const char*    key;
        MetadataValue  value;
        MetadataEntry* next;
    };

    struct Metadata
    {
        MetadataEntry* head;
        size_t         count;
    };

    Metadata* metadata_create(void);
    void      metadata_destroy(Metadata* meta);

    int metadata_set_string(Metadata* meta, const char* key, const char* value);
    int metadata_set_integer(Metadata* meta, const char* key, int64_t value);
    int metadata_set_double(Metadata* meta, const char* key, double value);
    int metadata_set_boolean(Metadata* meta, const char* key, int value);
    int metadata_set_binary(Metadata* meta, const char* key, const uint8_t* data, size_t length);

    int metadata_get_string(const Metadata* meta, const char* key, const char** value);
    int metadata_get_integer(const Metadata* meta, const char* key, int64_t* value);
    int metadata_get_double(const Metadata* meta, const char* key, double* value);
    int metadata_get_boolean(const Metadata* meta, const char* key, int* value);
    int metadata_get_binary(const Metadata* meta, const char* key, const uint8_t** data,
                            size_t* length);

    int    metadata_has_key(const Metadata* meta, const char* key);
    void   metadata_remove(Metadata* meta, const char* key);
    size_t metadata_count(const Metadata* meta);
    void   metadata_clear(Metadata* meta);

    Metadata* metadata_clone(const Metadata* meta);
    int       metadata_merge(Metadata* dest, const Metadata* src);

#ifdef __cplusplus
}
#endif

#endif // BS_KERNEL_IR_METADATA_H
