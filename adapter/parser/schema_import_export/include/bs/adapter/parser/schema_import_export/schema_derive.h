#ifndef BS_ADAPTER_PARSER_SCHEMA_DERIVE_H
#define BS_ADAPTER_PARSER_SCHEMA_DERIVE_H

/*
 * C-ST-7 contract block:
 * Thread safety: reentrant, no global state.
 * Error semantics: negative errno codes on error.
 * Platform notes: N/A.
 *
 * ADR-全链路接通 — Schema derivation from raw config values.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /** Field type inferred by schema derivation. */
    typedef enum bs_derived_type_t
    {
        BS_DTYPE_UNKNOWN = 0,
        BS_DTYPE_STRING,
        BS_DTYPE_INTEGER,
        BS_DTYPE_NUMBER,
        BS_DTYPE_BOOLEAN,
        BS_DTYPE_ARRAY,
        BS_DTYPE_OBJECT,
        BS_DTYPE_ENUM
    } bs_derived_type_t;

    /** A single derived schema field. */
    typedef struct bs_derived_field_t
    {
        char*             key;           /**< Field path (dot-notation) */
        bs_derived_type_t type;
        char*             example_value; /**< Example/observed value as JSON string */
        int               is_required;   /**< 1 if present in all samples */
        int               enum_candidate;/**< 1 if likely an enum */
    } bs_derived_field_t;

    /** Result of schema derivation. */
    typedef struct bs_derive_result_t
    {
        bs_derived_field_t* fields;
        size_t              count;
        char*               schema_json;  /**< Generated JSON Schema (Draft-07) */
    } bs_derive_result_t;

    /**
     * Derive a schema from a single v1 JSON config blob.
     *
     * Walks the JSON tree and infers field types from values.
     * Outputs both a structured array of field definitions and
     * a JSON Schema Draft-07 compatible string.
     *
     * @param v1_json     Input v1 JSON bytes.
     * @param v1_len      Input length.
     * @param out         Caller-freed result.
     * @return 0 on success, negative errno on error.
     */
    int bs_schema_derive(const uint8_t* v1_json, size_t v1_len,
                         bs_derive_result_t* out);

    /**
     * Free resources allocated by bs_schema_derive.
     */
    void bs_schema_derive_free(bs_derive_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PARSER_SCHEMA_DERIVE_H */
