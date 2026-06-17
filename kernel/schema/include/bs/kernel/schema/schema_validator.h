#ifndef BS_KERNEL_SCHEMA_VALIDATOR_H
#define BS_KERNEL_SCHEMA_VALIDATOR_H

/*
 * C-ST-7 contract block:
 * Thread safety: reentrant (no global state).
 * Error semantics: int return + bs_schema_validation_result_t for detailed errors.
 * Platform notes: Pure C; no external JSON library dependency.
 */

#include "schema_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** Lookup callback for custom C validators. Return NULL if not found. */
typedef bs_custom_validator_fn (*bs_validator_lookup_fn)(void* ctx, const char* name);

/**
 * Core validation entry point.
 * Validates config_value (parsed JSON tree) against the schema field definitions.
 *
 * @param fields        Schema field definitions (root level)
 * @param count         Number of root-level fields
 * @param config        Parsed config value (expects obj type at root)
 * @param opts          Validation options (NULL = collect-all default)
 * @param result        Output validation result (caller must free via bs_schema_validation_result_free)
 * @param parent_path   Parent path for dot-notation (pass "" for root)
 * @param lookup_fn     Custom validator lookup callback (may be NULL if no C registrations)
 * @param lookup_ctx    Context passed through to lookup_fn
 * @return              BS_SCHEMA_OK on success (check result->ok for validity),
 *                      negative on internal error
 */
    int bs_schema_validate_fields(
        const bs_schema_field_def_t* fields, size_t count,
        const bs_value_t* config,
        const bs_schema_validate_opts_t* opts,
        bs_schema_validation_result_t* result,
        const char* parent_path,
        bs_validator_lookup_fn lookup_fn,
        void* lookup_ctx);

/**
 * Validate a single value against a single field definition.
 * Used by bs_schema_validate_fields for each field.
 */
    int bs_schema_validate_single(
        const bs_schema_field_def_t* field_def,
        const bs_value_t* val,
        bs_schema_validation_result_t* result,
        const char* field_path,
        bs_validator_lookup_fn lookup_fn,
        void* lookup_ctx);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_SCHEMA_VALIDATOR_H */
