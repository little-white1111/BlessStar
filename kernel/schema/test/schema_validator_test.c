/* Validator tests */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bs/kernel/schema/schema_validator.h>
#include <bs/kernel/schema/schema_types.h>

static bs_schema_field_def_t make_field(const char* name, bs_schema_type_t type,
                                         int required, const char* ai_hint)
{
    bs_schema_field_def_t f;
    memset(&f, 0, sizeof(f));
    f.name     = name;
    f.type     = type;
    f.required = required ? true : false;
    f.ai_hint  = ai_hint ? ai_hint : "default hint text for testing";
    return f;
}

static bs_value_t make_str_val(const char* s)
{
    bs_value_t v;
    v.type = BS_VAL_STR;
    v.data.str_val = strdup(s);
    return v;
}

static bs_value_t make_i32_val(int32_t n)
{
    bs_value_t v;
    v.type = BS_VAL_I32;
    v.data.i32_val = n;
    return v;
}

static bs_value_t make_i64_val(int64_t n)
{
    bs_value_t v;
    v.type = BS_VAL_I64;
    v.data.i64_val = n;
    return v;
}

static bs_value_t make_f64_val(double d)
{
    bs_value_t v;
    v.type = BS_VAL_F64;
    v.data.f64_val = d;
    return v;
}

static bs_value_t make_bool_val(int b)
{
    bs_value_t v;
    v.type = BS_VAL_BOOL;
    v.data.bool_val = b;
    return v;
}

static bs_value_t make_null_val(void)
{
    bs_value_t v;
    v.type = BS_VAL_NULL;
    memset(&v.data, 0, sizeof(v.data));
    return v;
}

/* ── Tests ─────────────────────────────────────────────────────────── */

static int test_type_validation(void)
{
    printf("  test_type_validation ... ");
    bs_schema_field_def_t fd = make_field("val", BS_SCHEMA_TYPE_STR, 0, "a string field");

    bs_schema_validation_result_t res;
    memset(&res, 0, sizeof(res));
    res.ok = 1;

    /* str value on str field should pass */
    bs_value_t str_v = make_str_val("hello");
    int r = bs_schema_validate_single(&fd, &str_v, &res, "val", NULL, NULL);
    bs_value_free(&str_v);
    assert(r == BS_SCHEMA_OK);
    assert(res.ok == 1);

    /* i32 value on str field should fail type check */
    bs_value_t i32_v = make_i32_val(42);
    bs_schema_validation_result_t res2;
    memset(&res2, 0, sizeof(res2)); res2.ok = 1;
    r = bs_schema_validate_single(&fd, &i32_v, &res2, "val", NULL, NULL);
    bs_value_free(&i32_v);
    assert(r == BS_SCHEMA_OK); /* internal ok, but result shows failure */
    assert(res2.ok == 0);
    assert(res2.error_count > 0);
    assert(strcmp(res2.errors[0].rule_name, "type") == 0);
    bs_schema_validation_result_free(&res2);

    printf("OK\n");
    return 0;
}

static int test_required(void)
{
    printf("  test_required ... ");
    bs_schema_field_def_t fd = make_field("req", BS_SCHEMA_TYPE_STR, 1, "required field");

    /* We simulate missing field by passing a null value */
    bs_value_t null_v = make_null_val();
    bs_schema_validation_result_t res;
    memset(&res, 0, sizeof(res)); res.ok = 1;

    /* We need to test via validate_fields which checks required */
    bs_value_t obj_val;
    obj_val.type = BS_VAL_OBJ;
    obj_val.data.obj.fields = NULL;
    obj_val.data.obj.count = 0;

    int r = bs_schema_validate_fields(&fd, 1, &obj_val, NULL, &res, "", NULL, NULL);
    assert(r == BS_SCHEMA_OK);
    assert(res.ok == 0);
    assert(res.error_count > 0);
    assert(strcmp(res.errors[0].rule_name, "required") == 0);

    bs_schema_validation_result_free(&res);
    bs_value_free(&null_v);
    printf("OK\n");
    return 0;
}

static int test_range(void)
{
    printf("  test_range ... ");
    bs_schema_field_def_t fd = make_field("n", BS_SCHEMA_TYPE_I32, 0, "number 0-100");
    fd.range.has_min = true; fd.range.min = 0;
    fd.range.has_max = true; fd.range.max = 100;

    /* Valid */
    bs_value_t v = make_i32_val(50);
    bs_schema_validation_result_t res;
    memset(&res, 0, sizeof(res)); res.ok = 1;
    int r = bs_schema_validate_single(&fd, &v, &res, "n", NULL, NULL);
    bs_value_free(&v);
    assert(r == BS_SCHEMA_OK); assert(res.ok == 1);

    /* Below min */
    bs_value_t v2 = make_i32_val(-1);
    bs_schema_validation_result_t res2;
    memset(&res2, 0, sizeof(res2)); res2.ok = 1;
    r = bs_schema_validate_single(&fd, &v2, &res2, "n", NULL, NULL);
    bs_value_free(&v2);
    assert(res2.ok == 0);
    assert(strcmp(res2.errors[0].rule_name, "range") == 0);
    bs_schema_validation_result_free(&res2);

    printf("OK\n");
    return 0;
}

static int test_pattern(void)
{
    printf("  test_pattern ... ");
    bs_schema_field_def_t fd = make_field("email", BS_SCHEMA_TYPE_STR, 0, "email address");
    fd.pattern = "^[a-z]+@[a-z]+\\.[a-z]+$";

    bs_value_t v = make_str_val("user@example.com");
    bs_schema_validation_result_t res;
    memset(&res, 0, sizeof(res)); res.ok = 1;
    int r = bs_schema_validate_single(&fd, &v, &res, "email", NULL, NULL);
    bs_value_free(&v);
    assert(r == BS_SCHEMA_OK); assert(res.ok == 1);

    bs_value_t v2 = make_str_val("bad-email");
    bs_schema_validation_result_t res2;
    memset(&res2, 0, sizeof(res2)); res2.ok = 1;
    r = bs_schema_validate_single(&fd, &v2, &res2, "email", NULL, NULL);
    bs_value_free(&v2);
    assert(res2.ok == 0);
    assert(strcmp(res2.errors[0].rule_name, "pattern") == 0);
    bs_schema_validation_result_free(&res2);

    printf("OK\n");
    return 0;
}

static int test_enum(void)
{
    printf("  test_enum ... ");
    const char* enum_vals[] = {"admin", "user", "guest", NULL};
    bs_schema_field_def_t fd = make_field("role", BS_SCHEMA_TYPE_ENUM, 0, "user role");
    fd.enum_values = enum_vals;

    bs_value_t v = make_str_val("user");
    bs_schema_validation_result_t res;
    memset(&res, 0, sizeof(res)); res.ok = 1;
    int r = bs_schema_validate_single(&fd, &v, &res, "role", NULL, NULL);
    bs_value_free(&v);
    assert(r == BS_SCHEMA_OK); assert(res.ok == 1);

    bs_value_t v2 = make_str_val("superadmin");
    bs_schema_validation_result_t res2;
    memset(&res2, 0, sizeof(res2)); res2.ok = 1;
    r = bs_schema_validate_single(&fd, &v2, &res2, "role", NULL, NULL);
    bs_value_free(&v2);
    assert(res2.ok == 0);
    bs_schema_validation_result_free(&res2);

    printf("OK\n");
    return 0;
}

static int test_f64_range(void)
{
    printf("  test_f64_range ... ");
    bs_schema_field_def_t fd = make_field("p", BS_SCHEMA_TYPE_F64, 0, "price");
    fd.range.has_min = true; fd.range.min = 0.0;
    fd.range.has_max = true; fd.range.max = 9999.99;

    bs_value_t v = make_f64_val(123.45);
    bs_schema_validation_result_t res;
    memset(&res, 0, sizeof(res)); res.ok = 1;
    int r = bs_schema_validate_single(&fd, &v, &res, "p", NULL, NULL);
    bs_value_free(&v);
    assert(r == BS_SCHEMA_OK); assert(res.ok == 1);

    bs_value_t v2 = make_f64_val(10000.0);
    bs_schema_validation_result_t res2;
    memset(&res2, 0, sizeof(res2)); res2.ok = 1;
    r = bs_schema_validate_single(&fd, &v2, &res2, "p", NULL, NULL);
    bs_value_free(&v2);
    assert(res2.ok == 0);
    bs_schema_validation_result_free(&res2);

    printf("OK\n");
    return 0;
}

int main(void)
{
    printf("schema_validator_test:\n");
    int fail = 0;
    fail += test_type_validation();
    fail += test_required();
    fail += test_range();
    fail += test_pattern();
    fail += test_enum();
    fail += test_f64_range();

    printf("  %s\n", fail ? "FAILED" : "ALL PASS");
    return fail ? 1 : 0;
}
