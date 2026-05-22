#ifndef BS_ADAPTER_PARSER_SCHEMA_REGISTRY_H
#define BS_ADAPTER_PARSER_SCHEMA_REGISTRY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct SchemaRegistry SchemaRegistry;

    SchemaRegistry* schema_registry_create(void);
    void            schema_registry_destroy(SchemaRegistry* registry);

    int schema_registry_register(SchemaRegistry* registry, const char* name, const char* schema);
    int schema_registry_unregister(SchemaRegistry* registry, const char* name);
    const char* schema_registry_get(SchemaRegistry* registry, const char* name);

    int schema_registry_validate(SchemaRegistry* registry, const char* name, const char* data);
    int schema_registry_validate_data(SchemaRegistry* registry, const char* schema,
                                      const char* data);

    int    schema_registry_has_schema(SchemaRegistry* registry, const char* name);
    size_t schema_registry_get_count(const SchemaRegistry* registry);

    int  schema_registry_list_schemas(SchemaRegistry* registry, const char*** names, size_t* count);
    void schema_registry_free_names(const char*** names, size_t count);

#ifdef __cplusplus
}
#endif

#endif // BS_ADAPTER_PARSER_SCHEMA_REGISTRY_H
