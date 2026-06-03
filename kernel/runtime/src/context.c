#include "bs/kernel/common/bs_safe_format.h"
#include "bs/kernel/ir/Metadata.h"
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

Context* bs_context_create(ContextScope scope)
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

void bs_context_destroy(Context* ctx)
{
    if (!ctx)
        return;

    if (ctx->id)
        free((void*)ctx->id);

    if (ctx->metadata)
        bs_metadata_destroy(ctx->metadata);

    free(ctx);
}

const char* bs_context_get_id(const Context* ctx)
{
    return ctx ? ctx->id : NULL;
}

ContextScope bs_context_get_scope(const Context* ctx)
{
    return ctx ? ctx->scope : CONTEXT_SCOPE_GLOBAL;
}

int bs_context_set_metadata(Context* ctx, const char* key, const char* value)
{
    if (!ctx || !key || !value)
        return -1;

    if (!ctx->metadata)
    {
        ctx->metadata = bs_metadata_create();
        if (!ctx->metadata)
            return -1;
    }
    return bs_metadata_set_string(ctx->metadata, key, value);
}

const char* bs_context_get_metadata(const Context* ctx, const char* key)
{
    if (!ctx || !key || !ctx->metadata)
        return NULL;

    const char* value = NULL;
    if (bs_metadata_get_string(ctx->metadata, key, &value) != 0)
        return NULL;
    return value;
}

Metadata* bs_context_get_all_metadata(const Context* ctx)
{
    return ctx ? ctx->metadata : NULL;
}

void bs_context_set_user_data(Context* ctx, void* data)
{
    if (!ctx)
        return;
    ctx->user_data = data;
}

void* bs_context_get_user_data(const Context* ctx)
{
    return ctx ? ctx->user_data : NULL;
}

void bs_context_touch(Context* ctx)
{
    if (!ctx)
        return;
    ctx->last_accessed_at = (uint64_t)time(NULL);
}

uint64_t bs_context_get_age_ms(const Context* ctx)
{
    if (!ctx)
        return 0;
    return (uint64_t)time(NULL) - ctx->created_at;
}

Context* bs_context_clone(const Context* ctx)
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

int bs_context_merge(Context* dest, const Context* src)
{
    if (!dest || !src)
        return -1;

    // Merge metadata from src to dest
    (void)src;
    return 0;
}
