#include <bs/kernel/schema/schema_validator.h>
#include <bs/kernel/schema/schema_types.h>
#include <bs/kernel/schema/custom_validator.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "schema_compat.h"
#include "regex_compat.h"

/* ── Helper: append error ──────────────────────────────────────────── */
static int append_error(bs_schema_validation_result_t* result,
                         const char* field_path,
                         const char* rule_name,
                         const char* expected,
                         const char* actual,
                         const char* ai_hint)
{
    size_t new_count = result->error_count + 1;
    bs_schema_validation_error_t* new_errs = (bs_schema_validation_error_t*)
        realloc(result->errors, new_count * sizeof(bs_schema_validation_error_t));
    if (!new_errs) return BS_SCHEMA_ERR_NO_MEMORY;
    result->errors = new_errs;
    bs_schema_validation_error_t* e = &result->errors[result->error_count];
    e->field_path = bs_strdup(field_path ? field_path : "");
    e->rule_name  = bs_strdup(rule_name ? rule_name : "");
    e->expected   = bs_strdup(expected ? expected : "");
    e->actual     = bs_strdup(actual ? actual : "");
    e->ai_hint    = bs_strdup(ai_hint ? ai_hint : "");
    result->error_count++;
    result->ok = 0;
    return BS_SCHEMA_OK;
}

/* ── Value type name helpers ───────────────────────────────────────── */
static const char* schema_type_name(bs_schema_type_t t)
{
    switch (t)
    {
    case BS_SCHEMA_TYPE_STR:  return "str";
    case BS_SCHEMA_TYPE_I32:  return "i32";
    case BS_SCHEMA_TYPE_I64:  return "i64";
    case BS_SCHEMA_TYPE_F64:  return "f64";
    case BS_SCHEMA_TYPE_BOOL: return "bool";
    case BS_SCHEMA_TYPE_ARR:  return "arr";
    case BS_SCHEMA_TYPE_OBJ:  return "obj";
    case BS_SCHEMA_TYPE_ENUM: return "enum";
    default:                  return "unknown";
    }
}

static const char* value_type_name(bs_value_type_t t)
{
    switch (t)
    {
    case BS_VAL_NULL: return "null";
    case BS_VAL_BOOL: return "bool";
    case BS_VAL_I32:  return "i32";
    case BS_VAL_I64:  return "i64";
    case BS_VAL_F64:  return "f64";
    case BS_VAL_STR:  return "str";
    case BS_VAL_ARR:  return "arr";
    case BS_VAL_OBJ:  return "obj";
    default:          return "unknown";
    }
}

/* ── Type check ────────────────────────────────────────────────────── */
static bs_value_type_t value_type_for_schema_type(bs_schema_type_t st)
{
    switch (st)
    {
    case BS_SCHEMA_TYPE_STR:  return BS_VAL_STR;
    case BS_SCHEMA_TYPE_I32:  return BS_VAL_I32;
    case BS_SCHEMA_TYPE_I64:  return BS_VAL_I64;
    case BS_SCHEMA_TYPE_F64:  return BS_VAL_F64;
    case BS_SCHEMA_TYPE_BOOL: return BS_VAL_BOOL;
    case BS_SCHEMA_TYPE_ARR:  return BS_VAL_ARR;
    case BS_SCHEMA_TYPE_OBJ:  return BS_VAL_OBJ;
    default:                  return BS_VAL_NULL;
    }
}

/* ── Get field from object value by name ───────────────────────────── */
static const bs_value_t* find_field_value(const bs_value_t* obj_val,
                                           const char* name)
{
    if (obj_val->type != BS_VAL_OBJ) return NULL;
    for (size_t i = 0; i < obj_val->data.obj.count; i++)
    {
        if (strcmp(obj_val->data.obj.fields[i].name, name) == 0)
            return &obj_val->data.obj.fields[i].value;
    }
    return NULL;
}

/* ── Build dot-notation path ───────────────────────────────────────── */
static char* build_path(const char* parent, const char* name)
{
    if (!parent || parent[0] == '\0')
        return bs_strdup(name);
    size_t plen = strlen(parent);
    size_t nlen = strlen(name);
    char*  p    = (char*)malloc(plen + 1 + nlen + 1);
    if (!p) return NULL;
    memcpy(p, parent, plen);
    p[plen] = '.';
    memcpy(p + plen + 1, name, nlen);
    p[plen + 1 + nlen] = '\0';
    return p;
}

/* ── Numeric value helpers ─────────────────────────────────────────── */
static double value_to_double(const bs_value_t* val)
{
    switch (val->type)
    {
    case BS_VAL_I32: return (double)val->data.i32_val;
    case BS_VAL_I64: return (double)val->data.i64_val;
    case BS_VAL_F64: return val->data.f64_val;
    default:         return 0.0;
    }
}

/* ── Forward declaration for recursion ─────────────────────────────── */
static int validate_field_internal(
    const bs_schema_field_def_t* field_def,
    const bs_value_t* val,
    bs_schema_validation_result_t* result,
    const char* field_path,
    int fail_fast,
    bs_validator_lookup_fn lookup_fn,
    void* lookup_ctx);

/* ── Main exported function ────────────────────────────────────────── */
int bs_schema_validate_fields(
    const bs_schema_field_def_t* fields, size_t count,
    const bs_value_t* config,
    const bs_schema_validate_opts_t* opts,
    bs_schema_validation_result_t* result,
    const char* parent_path,
    bs_validator_lookup_fn lookup_fn,
    void* lookup_ctx)
{
    if (!fields || count == 0) return BS_SCHEMA_OK;
    if (!config) return BS_SCHEMA_ERR_INVALID_ARG;

    int fail_fast = (opts && opts->fail_fast) ? 1 : 0;

    /* Root should be an object */
    if (config->type != BS_VAL_OBJ)
    {
        append_error(result, parent_path, "type", "obj",
                     value_type_name(config->type), "");
        return fail_fast ? BS_SCHEMA_ERR_VALIDATION : BS_SCHEMA_OK;
    }

    for (size_t i = 0; i < count; i++)
    {
        const bs_schema_field_def_t* fd = &fields[i];
        char* full_path = build_path(parent_path, fd->name);
        const bs_value_t* field_val = find_field_value(config, fd->name);

        /* Required check */
        if (fd->required && (!field_val || field_val->type == BS_VAL_NULL))
        {
            append_error(result, full_path, "required",
                         "required field must be present and non-null",
                         field_val ? value_type_name(field_val->type) : "absent",
                         fd->ai_hint);
            free(full_path);
            if (fail_fast) return BS_SCHEMA_ERR_VALIDATION;
            continue;
        }

        if (!field_val || field_val->type == BS_VAL_NULL)
        {
            free(full_path);
            continue; /* optional, absent, skip */
        }

        /* Validate field */
        int ret = validate_field_internal(fd, field_val, result,
                                           full_path, fail_fast,
                                           lookup_fn, lookup_ctx);
        free(full_path);
        if (ret != BS_SCHEMA_OK && fail_fast)
            return ret;
    }

    return BS_SCHEMA_OK;
}

/* ── Single field validation ───────────────────────────────────────── */
int bs_schema_validate_single(
    const bs_schema_field_def_t* field_def,
    const bs_value_t* val,
    bs_schema_validation_result_t* result,
    const char* field_path,
    bs_validator_lookup_fn lookup_fn,
    void* lookup_ctx)
{
    return validate_field_internal(field_def, val, result,
                                    field_path,
                                    0, /* collect-all for single */
                                    lookup_fn, lookup_ctx);
}

/* ── Internal field validation ─────────────────────────────────────── */
static int validate_field_internal(
    const bs_schema_field_def_t* fd,
    const bs_value_t* val,
    bs_schema_validation_result_t* result,
    const char* field_path,
    int fail_fast,
    bs_validator_lookup_fn lookup_fn,
    void* lookup_ctx)
{
    /* 1. Type check */
    bs_value_type_t expected_vt = value_type_for_schema_type(fd->type);
    if (fd->type == BS_SCHEMA_TYPE_ENUM)
        expected_vt = BS_VAL_STR; /* enum values are strings internally */

    if (val->type != expected_vt)
    {
        char expected_buf[64];
        snprintf(expected_buf, sizeof(expected_buf), "type=%s", schema_type_name(fd->type));
        append_error(result, field_path, "type",
                     expected_buf,
                     value_type_name(val->type),
                     fd->ai_hint);
        if (fail_fast) return BS_SCHEMA_ERR_VALIDATION;
        /* For type mismatch, skip further checks on this field */
        return BS_SCHEMA_OK;
    }

    /* 2. Enum check */
    if (fd->type == BS_SCHEMA_TYPE_ENUM && fd->enum_values)
    {
        int matched = 0;
        for (int k = 0; fd->enum_values[k]; k++)
        {
            if (strcmp(val->data.str_val, fd->enum_values[k]) == 0)
            {
                matched = 1;
                break;
            }
        }
        if (!matched)
        {
            append_error(result, field_path, "enum",
                         "must be one of [enum values]",
                         val->data.str_val,
                         fd->ai_hint);
            if (fail_fast) return BS_SCHEMA_ERR_VALIDATION;
        }
    }

    /* 3. Range check (numeric types) */
    if ((fd->type == BS_SCHEMA_TYPE_I32 || fd->type == BS_SCHEMA_TYPE_I64 ||
         fd->type == BS_SCHEMA_TYPE_F64) &&
        (fd->range.has_min || fd->range.has_max))
    {
        double dval = value_to_double(val);
        if (fd->range.has_min && dval < fd->range.min - 1e-9)
        {
            char expected_buf[64];
            snprintf(expected_buf, sizeof(expected_buf), ">= %g", fd->range.min);
            char actual_buf[64];
            snprintf(actual_buf, sizeof(actual_buf), "%g", dval);
            append_error(result, field_path, "range",
                         expected_buf, actual_buf, fd->ai_hint);
            if (fail_fast) return BS_SCHEMA_ERR_VALIDATION;
        }
        if (fd->range.has_max && dval > fd->range.max + 1e-9)
        {
            char expected_buf[64];
            snprintf(expected_buf, sizeof(expected_buf), "<= %g", fd->range.max);
            char actual_buf[64];
            snprintf(actual_buf, sizeof(actual_buf), "%g", dval);
            append_error(result, field_path, "range",
                         expected_buf, actual_buf, fd->ai_hint);
            if (fail_fast) return BS_SCHEMA_ERR_VALIDATION;
        }
    }

    /* 4. Pattern check (string type) */
    if (fd->type == BS_SCHEMA_TYPE_STR && fd->pattern)
    {
        regex_t regex;
        int comp_ret = regcomp(&regex, fd->pattern, REG_EXTENDED | REG_NOSUB);
        if (comp_ret != 0)
        {
            /* Invalid pattern in schema definition — skip */
            regfree(&regex);
        }
        else
        {
            int match_ret = regexec(&regex, val->data.str_val, 0, NULL, 0);
            if (match_ret != 0)
            {
                append_error(result, field_path, "pattern",
                             fd->pattern,
                             val->data.str_val,
                             fd->ai_hint);
                regfree(&regex);
                if (fail_fast) return BS_SCHEMA_ERR_VALIDATION;
            }
            regfree(&regex);
        }
    }

    /* 5. Custom validator (expr or C fn) */
    if (fd->custom_validator)
    {
        int cv_ok = 0;
        int cv_err = 0;
        char err_buf[256];

        /* Try expr first */
        cv_ok = bs_custom_validator_eval_expr(fd->custom_validator, val, err_buf, sizeof(err_buf));
        if (cv_ok < 0)
        {
            /* expr parse failed — try as C function name */
            bs_custom_validator_fn cfn = NULL;
            if (lookup_fn)
                cfn = lookup_fn(lookup_ctx, fd->custom_validator);
            if (!cfn)
                bs_custom_validator_global_find(fd->custom_validator, &cfn);

            if (cfn)
            {
                cv_ok = cfn(val, err_buf, sizeof(err_buf));
                if (cv_ok == 0)
                    cv_err = 1;
            }
            else
            {
                /* Neither expr nor C fn work */
                snprintf(err_buf, sizeof(err_buf),
                         "custom_validator '%s' not recognized", fd->custom_validator);
                cv_err = 1;
            }
        }
        else if (cv_ok == 0)
        {
            cv_err = 1;
        }

        if (cv_err)
        {
            append_error(result, field_path, "custom",
                         fd->custom_validator,
                         err_buf,
                         fd->ai_hint);
            if (fail_fast) return BS_SCHEMA_ERR_VALIDATION;
        }
    }

    /* 6. Nested fields (obj type) */
    if (fd->type == BS_SCHEMA_TYPE_OBJ && fd->nested_fields && fd->nested_count > 0)
    {
        int ret = bs_schema_validate_fields(
            fd->nested_fields, fd->nested_count,
            val, NULL, result, field_path,
            lookup_fn, lookup_ctx);
        if (ret != BS_SCHEMA_OK && fail_fast)
            return ret;
    }

    /* 7. Array element validation */
    if (fd->type == BS_SCHEMA_TYPE_ARR && fd->elem_type != BS_SCHEMA_TYPE_STR)
    {
        for (size_t ei = 0; ei < val->data.arr.count; ei++)
        {
            char elem_path[1024];
            snprintf(elem_path, sizeof(elem_path), "%s[%zu]", field_path, ei);

            if (fd->elem_type == BS_SCHEMA_TYPE_OBJ && fd->elem_fields && fd->elem_nested_count > 0)
            {
                /* Build a temporary field def for the element */
                bs_schema_field_def_t elem_def;
                memset(&elem_def, 0, sizeof(elem_def));
                elem_def.type = BS_SCHEMA_TYPE_OBJ;
                elem_def.nested_fields = fd->elem_fields;
                elem_def.nested_count  = fd->elem_nested_count;
                elem_def.ai_hint       = fd->ai_hint;
                int ret = validate_field_internal(
                    &elem_def, &val->data.arr.items[ei],
                    result, elem_path, fail_fast,
                    lookup_fn, lookup_ctx);
                if (ret != BS_SCHEMA_OK && fail_fast)
                    return ret;
            }
        }
    }

    return BS_SCHEMA_OK;
}
