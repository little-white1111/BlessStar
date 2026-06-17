#ifndef BS_KERNEL_SCHEMA_REGISTRY_H
#define BS_KERNEL_SCHEMA_REGISTRY_H

/*
 * C-ST-7 contract block:
 * Thread safety: NOT thread-safe; callers must serialize access.
 * Error semantics: int return; BS_SCHEMA_OK (0) on success, negative on error.
 * Platform notes: Pure C; hash table backed inline.
 */

#include "schema_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* Opaque handle. */
typedef struct bs_schema_registry bs_schema_registry_t;

/* ── Lifecycle ─────────────────────────────────────────────────────── */
    bs_schema_registry_t* bs_schema_registry_create(void);
    void                  bs_schema_registry_destroy(bs_schema_registry_t* reg);

/* ── Schema registration ───────────────────────────────────────────── */
    int bs_schema_register(bs_schema_registry_t* reg,
                           const bs_schema_entry_t* entry);

    int bs_schema_unregister(bs_schema_registry_t* reg,
                             const char* schema_id, const char* version);

/* ── Schema lookup ─────────────────────────────────────────────────── */
    const bs_schema_entry_t* bs_schema_find(bs_schema_registry_t* reg,
                                             const char* schema_id,
                                             const char* version);

/* ── Meta query ────────────────────────────────────────────────────── */
    int bs_schema_get_meta(bs_schema_registry_t* reg,
                           const char* schema_id, const char* version,
                           /* out */ const bs_schema_ui_meta_t** meta);

/* ── Validation dispatch ───────────────────────────────────────────── */
    int bs_schema_validate(bs_schema_registry_t* reg,
                           const char* schema_id, const char* version,
                           const bs_value_t* config_value,
                           const bs_schema_validate_opts_t* opts,
                           /* out */ bs_schema_validation_result_t* result);

/* ── Custom validator registration ─────────────────────────────────── */
    int bs_schema_register_validator(bs_schema_registry_t* reg,
                                     const char* name,
                                     bs_custom_validator_fn fn);

    /**
     * Find registered C validator by name. Returns BS_SCHEMA_OK if found.
     * out_fn may be NULL (check existence only).
     */
    int bs_schema_find_validator(bs_schema_registry_t* reg,
                                 const char* name,
                                 bs_custom_validator_fn* out_fn);

/* ── Traversal / enumeration ────────────────────────────────────────── */
    typedef void (*bs_schema_foreach_fn)(const bs_schema_entry_t* entry,
                                          void* userdata);

    int    bs_schema_foreach(bs_schema_registry_t* reg,
                              bs_schema_foreach_fn fn, void* userdata);

    size_t bs_schema_count(bs_schema_registry_t* reg);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_SCHEMA_REGISTRY_H */
