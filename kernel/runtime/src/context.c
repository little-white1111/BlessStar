#include "bs/kernel/common/bs_safe_format.h"
#include "bs/kernel/runtime/Context.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char* generate_context_id(void)
{
    static unsigned long long counter = 0;
    char*                     id      = (char*)malloc(36);
    if (!id)
        return NULL;

    bs_safe_snprintf(id, 36, "ctx-%llu-%llu", (unsigned long long)time(NULL), counter++);
    return id;
}

Context* context_create(ContextScope scope)
{
    Context* ctx = (Context*)malloc(sizeof(Context));
    if (!ctx)
        return NULL;

    ctx->id               = generate_context_id();
    ctx->scope            = scope;
    ctx->created_at       = (uint64_t)time(NULL);
    ctx->last_accessed_at = ctx->created_at;
    ctx->metadata         = NULL;
    ctx->user_data        = NULL;

    return ctx;
}

void context_destroy(Context* ctx)
{
    if (!ctx)
        return;

    if (ctx->id)
        free(ctx->id);

    // Clean up metadata if implemented
    if (ctx->metadata)
    {
        // Assume Metadata has a destroy function
        // metadata_destroy(ctx->metadata);
    }

    free(ctx);
}

const char* context_get_id(const Context* ctx)
{
    return ctx ? ctx->id : NULL;
}

ContextScope context_get_scope(const Context* ctx)
{
    return ctx ? ctx->scope : CONTEXT_SCOPE_GLOBAL;
}

int context_set_metadata(Context* ctx, const char* key, const char* value)
{
    if (!ctx || !key || !value)
        return -1;

    // Simple implementation: store as key-value pairs
    // This would be implemented with the Metadata struct
    (void)key;
    (void)value;
    return 0;
}

const char* context_get_metadata(const Context* ctx, const char* key)
{
    if (!ctx || !key)
        return NULL;

    // Simple implementation: retrieve key-value pairs
    (void)key;
    return NULL;
}

void* context_get_all_metadata(const Context* ctx)
{
    return ctx ? ctx->metadata : NULL;
}

void context_set_user_data(Context* ctx, void* data)
{
    if (!ctx)
        return;
    ctx->user_data = data;
}

void* context_get_user_data(const Context* ctx)
{
    return ctx ? ctx->user_data : NULL;
}

void context_touch(Context* ctx)
{
    if (!ctx)
        return;
    ctx->last_accessed_at = (uint64_t)time(NULL);
}

uint64_t context_get_age_ms(const Context* ctx)
{
    if (!ctx)
        return 0;
    return (uint64_t)time(NULL) - ctx->created_at;
}

Context* context_clone(const Context* ctx)
{
    if (!ctx)
        return NULL;

    Context* clone = (Context*)malloc(sizeof(Context));
    if (!clone)
        return NULL;

    clone->id               = generate_context_id();
    clone->scope            = ctx->scope;
    clone->created_at       = (uint64_t)time(NULL);
    clone->last_accessed_at = clone->created_at;
    clone->metadata         = NULL; // Would need deep copy
    clone->user_data        = NULL; // User data not cloned

    return clone;
}

int context_merge(Context* dest, const Context* src)
{
    if (!dest || !src)
        return -1;

    // Merge metadata from src to dest
    (void)src;
    return 0;
}
