#ifndef BS_KERNEL_RUNTIME_CONTEXT_H
#define BS_KERNEL_RUNTIME_CONTEXT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct Metadata Metadata;
    typedef struct Context  Context;

    typedef enum ContextScope
    {
        CONTEXT_SCOPE_GLOBAL,
        CONTEXT_SCOPE_SESSION,
        CONTEXT_SCOPE_REQUEST
    } ContextScope;

    struct Context
    {
        const char*  id;
        ContextScope scope;
        uint64_t     created_at;
        uint64_t     last_accessed_at;
        Metadata*    metadata;
        void*        user_data;
    };

    Context* context_create(ContextScope scope);
    void     context_destroy(Context* context);

    const char*  context_get_id(const Context* context);
    ContextScope context_get_scope(const Context* context);

    int         context_set_metadata(Context* context, const char* key, const char* value);
    const char* context_get_metadata(const Context* context, const char* key);
    Metadata*   context_get_all_metadata(const Context* context);

    void  context_set_user_data(Context* context, void* data);
    void* context_get_user_data(const Context* context);

    void     context_touch(Context* context);
    uint64_t context_get_age_ms(const Context* context);

    Context* context_clone(const Context* context);
    int      context_merge(Context* dest, const Context* src);

#ifdef __cplusplus
}
#endif

#endif // BS_KERNEL_RUNTIME_CONTEXT_H
