#include <bs/kernel/schema/schema_types.h>
#include <stdlib.h>
#include <string.h>

void bs_value_free(bs_value_t* val)
{
    if (!val) return;
    switch (val->type)
    {
    case BS_VAL_STR:
        free(val->data.str_val);
        break;
    case BS_VAL_ARR:
        for (size_t i = 0; i < val->data.arr.count; i++)
            bs_value_free(&val->data.arr.items[i]);
        free(val->data.arr.items);
        break;
    case BS_VAL_OBJ:
        for (size_t i = 0; i < val->data.obj.count; i++)
        {
            free(val->data.obj.fields[i].name);
            bs_value_free(&val->data.obj.fields[i].value);
        }
        free(val->data.obj.fields);
        break;
    default:
        break;
    }
    val->type = BS_VAL_NULL;
}

void bs_field_free(bs_field_t* f)
{
    if (!f) return;
    free(f->name);
    bs_value_free(&f->value);
}

static void field_def_free(bs_schema_field_def_t* def)
{
    if (!def) return;
    free((void*)def->enum_values);
    if (def->nested_fields && def->nested_count > 0)
    {
        for (size_t j = 0; j < def->nested_count; j++)
            field_def_free(&def->nested_fields[j]);
        free(def->nested_fields);
        def->nested_fields = NULL;
    }
    if (def->elem_fields && def->elem_nested_count > 0)
    {
        for (size_t j = 0; j < def->elem_nested_count; j++)
            field_def_free(&def->elem_fields[j]);
        free(def->elem_fields);
        def->elem_fields = NULL;
    }
}

void bs_schema_field_def_free(bs_schema_field_def_t* def, size_t count)
{
    if (!def) return;
    for (size_t i = 0; i < count; i++)
        field_def_free(&def[i]);
    free(def);
}

void bs_schema_validation_error_free(bs_schema_validation_error_t* err)
{
    if (!err) return;
    free(err->field_path);
    free(err->rule_name);
    free(err->expected);
    free(err->actual);
    free(err->ai_hint);
}

void bs_schema_validation_result_free(bs_schema_validation_result_t* res)
{
    if (!res) return;
    for (size_t i = 0; i < res->error_count; i++)
        bs_schema_validation_error_free(&res->errors[i]);
    free(res->errors);
    res->errors      = NULL;
    res->error_count = 0;
}
