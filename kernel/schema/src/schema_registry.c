#include <bs/kernel/schema/schema_registry.h>
#include <bs/kernel/schema/schema_validator.h>
#include <bs/kernel/schema/custom_validator.h>

#include "schema_compat.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Internal data structures ──────────────────────────────────────── */
#define BS_SCHEMA_REGISTRY_HASH_SIZE 64

typedef struct schema_node
{
    struct schema_node* next;
    bs_schema_entry_t   entry; /* owned */
} schema_node_t;

/* Flat custom-validator hash-table slot */
typedef struct
{
    char*                name;  /* owned */
    bs_custom_validator_fn fn;
    int                  used;
} cfn_slot_t;

struct bs_schema_registry
{
    schema_node_t* buckets[BS_SCHEMA_REGISTRY_HASH_SIZE];
    cfn_slot_t     cfn_table[BS_SCHEMA_REGISTRY_HASH_SIZE];
};

/* ── String hash ───────────────────────────────────────────────────── */
static unsigned int hash_str(const char* s)
{
    unsigned int h = 2166136261u;
    while (*s)
    {
        h ^= (unsigned char)*s++;
        h *= 16777619u;
    }
    return h;
}

static unsigned int bucket_for(const char* id, const char* ver)
{
    size_t il = strlen(id);
    size_t vl = strlen(ver);
    char*  buf = (char*)malloc(il + vl + 2);
    if (!buf) return 0;
    memcpy(buf, id, il);
    buf[il] = '|';
    memcpy(buf + il + 1, ver, vl);
    buf[il + vl + 1] = '\0';
    unsigned int h = hash_str(buf) % BS_SCHEMA_REGISTRY_HASH_SIZE;
    free(buf);
    return h;
}

/* ── Create / Destroy ──────────────────────────────────────────────── */
bs_schema_registry_t* bs_schema_registry_create(void)
{
    bs_schema_registry_t* r = (bs_schema_registry_t*)
        calloc(1, sizeof(bs_schema_registry_t));
    return r;
}

static void free_entry_contents(bs_schema_entry_t* e)
{
    if (!e) return;
    free(e->schema_id);
    e->schema_id = NULL;
    free(e->version);
    e->version = NULL;
    /* enum_values in field defs are deep-copied.
     * field_def_free handles nested + enum_values */
    if (e->root_fields)
    {
        for (size_t i = 0; i < e->root_count; i++)
        {
            bs_schema_field_def_t* fd = &e->root_fields[i];
            /* Free nested deep copies */
            free((void*)fd->enum_values);
            if (fd->nested_fields && fd->nested_count > 0)
            {
                for (size_t j = 0; j < fd->nested_count; j++)
                {
                    free((void*)fd->nested_fields[j].enum_values);
                }
                free(fd->nested_fields);
            }
            if (fd->elem_fields && fd->elem_nested_count > 0)
            {
                for (size_t j = 0; j < fd->elem_nested_count; j++)
                {
                    free((void*)fd->elem_fields[j].enum_values);
                }
                free(fd->elem_fields);
            }
        }
        free(e->root_fields);
        e->root_fields = NULL;
    }
    e->root_count = 0;
}

void bs_schema_registry_destroy(bs_schema_registry_t* reg)
{
    if (!reg) return;
    for (int i = 0; i < BS_SCHEMA_REGISTRY_HASH_SIZE; i++)
    {
        schema_node_t* n = reg->buckets[i];
        while (n)
        {
            schema_node_t* next = n->next;
            free_entry_contents(&n->entry);
            free(n);
            n = next;
        }
        free(reg->cfn_table[i].name);
    }
    free(reg);
}

/* ── Find node (internal) ──────────────────────────────────────────── */
static schema_node_t* find_node(bs_schema_registry_t* reg,
                                 const char* id, const char* ver)
{
    unsigned int bi = bucket_for(id, ver);
    schema_node_t* n = reg->buckets[bi];
    while (n)
    {
        if (strcmp(n->entry.schema_id, id) == 0 &&
            strcmp(n->entry.version, ver) == 0)
            return n;
        n = n->next;
    }
    return NULL;
}

/* ── ai_hint recursive check ───────────────────────────────────────── */
static int check_ai_hint(const bs_schema_field_def_t* f)
{
    if (!f->ai_hint) return BS_SCHEMA_ERR_INVALID_ARG;
    size_t len = strlen(f->ai_hint);
    if (len < 4)  return BS_SCHEMA_ERR_AI_HINT_TOO_SHORT;
    if (len > 1024) return BS_SCHEMA_ERR_AI_HINT_TOO_LONG;
    return BS_SCHEMA_OK;
}

static int check_ai_hint_recursive(const bs_schema_field_def_t* flds, size_t cnt)
{
    for (size_t i = 0; i < cnt; i++)
    {
        int r = check_ai_hint(&flds[i]);
        if (r) return r;
        if (flds[i].nested_fields && flds[i].nested_count > 0)
        {
            r = check_ai_hint_recursive(flds[i].nested_fields, flds[i].nested_count);
            if (r) return r;
        }
        if (flds[i].elem_fields && flds[i].elem_nested_count > 0)
        {
            r = check_ai_hint_recursive(flds[i].elem_fields, flds[i].elem_nested_count);
            if (r) return r;
        }
    }
    return BS_SCHEMA_OK;
}

/* ── Deep-copy field_def (recursive, keeps string pointers from caller) ── */
/* We only deep-copy nested arrays and enum_values. Strings remain as
 * const char* from caller for MVP. */
static int deep_copy_fields(const bs_schema_field_def_t* src,
                             bs_schema_field_def_t** out, size_t count)
{
    *out = (bs_schema_field_def_t*)calloc(count, sizeof(bs_schema_field_def_t));
    if (!*out) return BS_SCHEMA_ERR_NO_MEMORY;

    for (size_t i = 0; i < count; i++)
    {
        (*out)[i] = src[i]; /* shallow copy */

        /* Deep-copy enum_values */
        if (src[i].enum_values)
        {
            int ec = 0;
            while (src[i].enum_values[ec]) ec++;
            const char** ev = (const char**)calloc(ec + 1, sizeof(char*));
            if (!ev) return BS_SCHEMA_ERR_NO_MEMORY;
            for (int j = 0; j < ec; j++)
                ev[j] = src[i].enum_values[j];
            ev[ec] = NULL;
            (*out)[i].enum_values = ev;
        }

        /* Deep-copy nested */
        if (src[i].nested_fields && src[i].nested_count > 0)
        {
            int r = deep_copy_fields(src[i].nested_fields,
                                      &(*out)[i].nested_fields,
                                      src[i].nested_count);
            if (r) return r;
        }

        /* Deep-copy elem_fields */
        if (src[i].elem_fields && src[i].elem_nested_count > 0)
        {
            int r = deep_copy_fields(src[i].elem_fields,
                                      &(*out)[i].elem_fields,
                                      src[i].elem_nested_count);
            if (r) return r;
        }
    }
    return BS_SCHEMA_OK;
}

/* ── Register ──────────────────────────────────────────────────────── */
int bs_schema_register(bs_schema_registry_t* reg,
                       const bs_schema_entry_t* entry)
{
    if (!reg || !entry || !entry->schema_id || !entry->version)
        return BS_SCHEMA_ERR_INVALID_ARG;

    int hr = check_ai_hint_recursive(entry->root_fields, entry->root_count);
    if (hr) return hr;

    if (find_node(reg, entry->schema_id, entry->version))
        return BS_SCHEMA_ERR_ALREADY_EXISTS;

    bs_schema_entry_t copy;
    memset(&copy, 0, sizeof(copy));
    copy.schema_id = bs_strdup(entry->schema_id);
    copy.version   = bs_strdup(entry->version);
    if (!copy.schema_id || !copy.version)
    {
        free(copy.schema_id);
        free(copy.version);
        return BS_SCHEMA_ERR_NO_MEMORY;
    }
    copy.root_count = entry->root_count;
    int dc = deep_copy_fields(entry->root_fields, &copy.root_fields, entry->root_count);
    if (dc)
    {
        free(copy.schema_id);
        free(copy.version);
        return dc;
    }
    copy.ui_meta = entry->ui_meta;

    unsigned int bi = bucket_for(entry->schema_id, entry->version);
    schema_node_t* node = (schema_node_t*)calloc(1, sizeof(schema_node_t));
    if (!node)
    {
        free_entry_contents(&copy);
        return BS_SCHEMA_ERR_NO_MEMORY;
    }
    node->entry = copy;
    node->next  = reg->buckets[bi];
    reg->buckets[bi] = node;
    return BS_SCHEMA_OK;
}

/* ── Unregister ────────────────────────────────────────────────────── */
int bs_schema_unregister(bs_schema_registry_t* reg,
                          const char* id, const char* ver)
{
    if (!reg || !id || !ver) return BS_SCHEMA_ERR_INVALID_ARG;
    unsigned int bi = bucket_for(id, ver);
    schema_node_t* prev = NULL;
    schema_node_t* n    = reg->buckets[bi];
    while (n)
    {
        if (strcmp(n->entry.schema_id, id) == 0 &&
            strcmp(n->entry.version, ver) == 0)
        {
            if (prev) prev->next = n->next;
            else      reg->buckets[bi] = n->next;
            free_entry_contents(&n->entry);
            free(n);
            return BS_SCHEMA_OK;
        }
        prev = n;
        n    = n->next;
    }
    return BS_SCHEMA_ERR_NOT_FOUND;
}

/* ── Find ──────────────────────────────────────────────────────────── */
const bs_schema_entry_t* bs_schema_find(bs_schema_registry_t* reg,
                                          const char* id, const char* ver)
{
    if (!reg || !id || !ver) return NULL;
    schema_node_t* n = find_node(reg, id, ver);
    return n ? &n->entry : NULL;
}

/* ── Get meta ──────────────────────────────────────────────────────── */
int bs_schema_get_meta(bs_schema_registry_t* reg,
                        const char* id, const char* ver,
                        const bs_schema_ui_meta_t** meta)
{
    if (!reg || !id || !ver || !meta)
        return BS_SCHEMA_ERR_INVALID_ARG;
    schema_node_t* n = find_node(reg, id, ver);
    if (!n) return BS_SCHEMA_ERR_NOT_FOUND;
    *meta = &n->entry.ui_meta;
    return BS_SCHEMA_OK;
}

/* ── Lookup callback for validator ─────────────────────────────────── */
static bs_custom_validator_fn cfn_lookup(void* ctx, const char* name)
{
    bs_schema_registry_t* reg = (bs_schema_registry_t*)ctx;
    if (!reg || !name) return NULL;
    unsigned int bi = hash_str(name) % BS_SCHEMA_REGISTRY_HASH_SIZE;
    /* Linear probe in the bucket */
    int start = (int)bi;
    for (int i = 0; i < BS_SCHEMA_REGISTRY_HASH_SIZE; i++)
    {
        int idx = (start + i) % BS_SCHEMA_REGISTRY_HASH_SIZE;
        if (!reg->cfn_table[idx].used) continue;
        if (strcmp(reg->cfn_table[idx].name, name) == 0)
            return reg->cfn_table[idx].fn;
    }
    return NULL;
}

/* ── Validate ──────────────────────────────────────────────────────── */
int bs_schema_validate(bs_schema_registry_t* reg,
                        const char* id, const char* ver,
                        const bs_value_t* config_value,
                        const bs_schema_validate_opts_t* opts,
                        bs_schema_validation_result_t* result)
{
    if (!reg || !id || !ver || !config_value || !result)
        return BS_SCHEMA_ERR_INVALID_ARG;
    schema_node_t* n = find_node(reg, id, ver);
    if (!n) return BS_SCHEMA_ERR_NOT_FOUND;

    bs_schema_validate_opts_t actual_opts;
    if (opts)
        actual_opts = *opts;
    else
    {
        actual_opts.fail_fast = false; /* collect-all default */
    }

    result->ok = 1;
    result->errors = NULL;
    result->error_count = 0;

    return bs_schema_validate_fields(
        n->entry.root_fields, n->entry.root_count,
        config_value, &actual_opts, result, "",
        cfn_lookup, reg);
}

/* ── Register / Find validator ─────────────────────────────────────── */
int bs_schema_register_validator(bs_schema_registry_t* reg,
                                  const char* name,
                                  bs_custom_validator_fn fn)
{
    if (!reg || !name || !fn) return BS_SCHEMA_ERR_INVALID_ARG;

    /* Check if already exists */
    bs_custom_validator_fn dummy;
    if (bs_schema_find_validator(reg, name, &dummy) == BS_SCHEMA_OK)
        return BS_SCHEMA_ERR_ALREADY_EXISTS;

    unsigned int bi = hash_str(name) % BS_SCHEMA_REGISTRY_HASH_SIZE;
    /* Find free slot in this bucket (linear probe) */
    int start = (int)bi;
    for (int i = 0; i < BS_SCHEMA_REGISTRY_HASH_SIZE; i++)
    {
        int idx = (start + i) % BS_SCHEMA_REGISTRY_HASH_SIZE;
        if (!reg->cfn_table[idx].used)
        {
            reg->cfn_table[idx].name = bs_strdup(name);
            if (!reg->cfn_table[idx].name)
                return BS_SCHEMA_ERR_NO_MEMORY;
            reg->cfn_table[idx].fn   = fn;
            reg->cfn_table[idx].used = 1;
            return BS_SCHEMA_OK;
        }
    }
    return BS_SCHEMA_ERR_NO_MEMORY;
}

int bs_schema_find_validator(bs_schema_registry_t* reg,
                              const char* name,
                              bs_custom_validator_fn* out_fn)
{
    if (!reg || !name) return BS_SCHEMA_ERR_INVALID_ARG;
    bs_custom_validator_fn fn = cfn_lookup(reg, name);
    if (!fn) return BS_SCHEMA_ERR_NOT_FOUND;
    if (out_fn) *out_fn = fn;
    return BS_SCHEMA_OK;
}

/* ── Traversal ──────────────────────────────────────────────────────── */
int bs_schema_foreach(bs_schema_registry_t* reg,
                       bs_schema_foreach_fn fn, void* userdata)
{
    if (!reg || !fn) return BS_SCHEMA_ERR_INVALID_ARG;
    for (int i = 0; i < BS_SCHEMA_REGISTRY_HASH_SIZE; i++)
    {
        schema_node_t* n = reg->buckets[i];
        while (n)
        {
            fn(&n->entry, userdata);
            n = n->next;
        }
    }
    return BS_SCHEMA_OK;
}

size_t bs_schema_count(bs_schema_registry_t* reg)
{
    if (!reg) return 0;
    size_t cnt = 0;
    for (int i = 0; i < BS_SCHEMA_REGISTRY_HASH_SIZE; i++)
    {
        schema_node_t* n = reg->buckets[i];
        while (n)
        {
            cnt++;
            n = n->next;
        }
    }
    return cnt;
}
