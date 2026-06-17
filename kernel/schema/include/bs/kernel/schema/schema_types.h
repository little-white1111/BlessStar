#ifndef BS_KERNEL_SCHEMA_TYPES_H
#define BS_KERNEL_SCHEMA_TYPES_H

/*
 * C-ST-7 contract block:
 * Thread safety: types only; immutable after creation.
 * Error semantics: N/A (types/structs only).
 * Platform notes: Pure C shared types for schema registry, validator, and converter.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ── Schema field types ────────────────────────────────────────────── */
    typedef enum bs_schema_type
    {
        BS_SCHEMA_TYPE_STR  = 0,
        BS_SCHEMA_TYPE_I32  = 1,
        BS_SCHEMA_TYPE_I64  = 2,
        BS_SCHEMA_TYPE_F64  = 3,
        BS_SCHEMA_TYPE_BOOL = 4,
        BS_SCHEMA_TYPE_ARR  = 5,
        BS_SCHEMA_TYPE_OBJ  = 6,
        BS_SCHEMA_TYPE_ENUM = 7
    } bs_schema_type_t;

/* ── Config value types (for validation input) ─────────────────────── */
    typedef enum bs_value_type
    {
        BS_VAL_NULL = 0,
        BS_VAL_BOOL = 1,
        BS_VAL_I32  = 2,
        BS_VAL_I64  = 3,
        BS_VAL_F64  = 4,
        BS_VAL_STR  = 5,
        BS_VAL_ARR  = 6,
        BS_VAL_OBJ  = 7
    } bs_value_type_t;

/* Forward declaration. */
struct bs_schema_field_def;
struct bs_value;
struct bs_field;

/* Value union for validator input. */
    typedef struct bs_value
    {
        bs_value_type_t type;
        union
        {
            bool         bool_val;
            int32_t      i32_val;
            int64_t      i64_val;
            double       f64_val;
            char*        str_val;       /* owned */
            struct
            {
                struct bs_value* items;
                size_t           count;
            } arr;
            struct
            {
                struct bs_field* fields;
                size_t           count;
            } obj;
        } data;
    } bs_value_t;

    typedef struct bs_field
    {
        char*       name;  /* owned */
        bs_value_t  value;
    } bs_field_t;

/* ── Schema field definition (programmatic API) ────────────────────── */
    typedef struct bs_range_def
    {
        double min;
        double max;
        bool   has_min;
        bool   has_max;
    } bs_range_def_t;

    typedef struct bs_schema_field_def
    {
        const char*                 name;             /* field name (not owned) */
        bs_schema_type_t            type;

        bool                        required;
        bs_range_def_t              range;
        const char*                 pattern;          /* regex string */
        const char* const*          enum_values;      /* NULL-terminated array for ENUM type */
        const char*                 custom_validator; /* expr string or C fn name */

        const char*                 ai_hint;          /* mandatory: 4-1024 chars */

        /* UI metadata */
        const char*                 ui_label;
        const char*                 ui_description;
        const char*                 ui_placeholder;
        int                         ui_order;

        /* Nested fields (for OBJ type) */
        struct bs_schema_field_def* nested_fields;
        size_t                      nested_count;

        /* Array element info */
        bs_schema_type_t            elem_type;
        struct bs_schema_field_def* elem_fields;      /* if elem_type == OBJ */
        size_t                      elem_nested_count;
    } bs_schema_field_def_t;

/* ── UI metadata block ─────────────────────────────────────────────── */
    typedef struct bs_schema_ui_meta
    {
        const char* title;
        const char* description;
    } bs_schema_ui_meta_t;

/* ── Schema entry (stored in registry) ─────────────────────────────── */
    typedef struct bs_schema_entry
    {
        char*                   schema_id;       /* owned */
        char*                   version;         /* owned */
        bs_schema_field_def_t*  root_fields;     /* owned, array */
        size_t                  root_count;
        bs_schema_ui_meta_t    ui_meta;
    } bs_schema_entry_t;

/* ── Custom validator function pointer ─────────────────────────────── */
    typedef int (*bs_custom_validator_fn)(const bs_value_t* val,
                                          char* err_buf, size_t err_sz);

/* ── Validation error ──────────────────────────────────────────────── */
    typedef struct bs_schema_validation_error
    {
        char* field_path;    /* dot-notation, owned */
        char* rule_name;     /* "type"/"required"/"range"/"pattern"/"enum"/"custom", owned */
        char* expected;      /* expected value description, owned */
        char* actual;        /* actual value description, owned */
        char* ai_hint;       /* field's ai_hint, owned */
    } bs_schema_validation_error_t;

/* ── Validation options ────────────────────────────────────────────── */
    typedef struct bs_schema_validate_opts
    {
        bool fail_fast;    /* true: stop on first error; false: collect all */
    } bs_schema_validate_opts_t;

/* ── Validation result ─────────────────────────────────────────────── */
    typedef struct bs_schema_validation_result
    {
        int                           ok;           /* 1 = valid, 0 = invalid */
        bs_schema_validation_error_t* errors;       /* owned array */
        size_t                        error_count;
    } bs_schema_validation_result_t;

/* ── Status codes ──────────────────────────────────────────────────── */
#define BS_SCHEMA_OK                    0
#define BS_SCHEMA_ERR_INVALID_ARG       -1
#define BS_SCHEMA_ERR_ALREADY_EXISTS    -2
#define BS_SCHEMA_ERR_NOT_FOUND         -3
#define BS_SCHEMA_ERR_AI_HINT_TOO_SHORT -4
#define BS_SCHEMA_ERR_AI_HINT_TOO_LONG  -5
#define BS_SCHEMA_ERR_VALIDATION        -6
#define BS_SCHEMA_ERR_PARSE             -7
#define BS_SCHEMA_ERR_NO_MEMORY         -8

/* ── Lifecycle helpers ─────────────────────────────────────────────── */
    void bs_value_free(bs_value_t* val);
    void bs_field_free(bs_field_t* f);
    void bs_schema_field_def_free(bs_schema_field_def_t* def, size_t count);
    void bs_schema_validation_error_free(bs_schema_validation_error_t* err);
    void bs_schema_validation_result_free(bs_schema_validation_result_t* res);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_SCHEMA_TYPES_H */
