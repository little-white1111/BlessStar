/* Custom validator tests */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bs/kernel/schema/custom_validator.h>
#include <bs/kernel/schema/schema_types.h>

static bs_value_t make_i32_val(int32_t n)
{
    bs_value_t v;
    v.type = BS_VAL_I32;
    v.data.i32_val = n;
    return v;
}

static bs_value_t make_f64_val(double d)
{
    bs_value_t v;
    v.type = BS_VAL_F64;
    v.data.f64_val = d;
    return v;
}

static bs_value_t make_str_val(const char* s)
{
    bs_value_t v;
    v.type = BS_VAL_STR;
    v.data.str_val = strdup(s);
    return v;
}

/* ── Test C validator function ─────────────────────────────────────── */
static int test_cfn_positive(const bs_value_t* val,
                              char* err_buf, size_t err_sz)
{
    if (!val) return 0;
    double d = 0;
    if (val->type == BS_VAL_I32) d = val->data.i32_val;
    else if (val->type == BS_VAL_F64) d = val->data.f64_val;
    else { if (err_buf && err_sz) snprintf(err_buf, err_sz, "not numeric"); return 0; }
    if (d > 0) return 1;
    if (err_buf && err_sz) snprintf(err_buf, err_sz, "value must be positive");
    return 0;
}

/* ── Tests ─────────────────────────────────────────────────────────── */

static int test_expr_arithmetic(void)
{
    printf("  test_expr_arithmetic ... ");
    bs_value_t v = make_i32_val(42);

    char err[256];
    /* value > 0 */
    int r = bs_custom_validator_eval_expr("value > 0", &v, err, sizeof(err));
    assert(r == 1);

    /* value == 42 */
    r = bs_custom_validator_eval_expr("value == 42", &v, err, sizeof(err));
    assert(r == 1);

    /* value != 0 */
    r = bs_custom_validator_eval_expr("value != 0", &v, err, sizeof(err));
    assert(r == 1);

    /* value > 100 */
    r = bs_custom_validator_eval_expr("value > 100", &v, err, sizeof(err));
    assert(r == 0);

    bs_value_free(&v);
    printf("OK\n");
    return 0;
}

static int test_expr_logical(void)
{
    printf("  test_expr_logical ... ");
    bs_value_t v = make_i32_val(50);

    char err[256];
    int r = bs_custom_validator_eval_expr("value > 0 && value < 100", &v, err, sizeof(err));
    assert(r == 1);

    r = bs_custom_validator_eval_expr("value > 100 || value < 0", &v, err, sizeof(err));
    assert(r == 0);

    r = bs_custom_validator_eval_expr("!(value == 0)", &v, err, sizeof(err));
    assert(r == 1);

    bs_value_free(&v);
    printf("OK\n");
    return 0;
}

static int test_expr_grouping(void)
{
    printf("  test_expr_grouping ... ");
    bs_value_t v = make_i32_val(10);

    char err[256];
    int r = bs_custom_validator_eval_expr("(value + 5) > 10", &v, err, sizeof(err));
    assert(r == 1);

    r = bs_custom_validator_eval_expr("(value - 5) > 10", &v, err, sizeof(err));
    assert(r == 0);

    bs_value_free(&v);
    printf("OK\n");
    return 0;
}

static int test_expr_float(void)
{
    printf("  test_expr_float ... ");
    bs_value_t v = make_f64_val(3.14);

    char err[256];
    int r = bs_custom_validator_eval_expr("value > 3.0 && value < 4.0", &v, err, sizeof(err));
    assert(r == 1);

    r = bs_custom_validator_eval_expr("value > 5.0", &v, err, sizeof(err));
    assert(r == 0);

    bs_value_free(&v);
    printf("OK\n");
    return 0;
}

static int test_cfn_global(void)
{
    printf("  test_cfn_global ... ");
    int r = bs_custom_validator_global_register("is_positive", test_cfn_positive);
    assert(r == BS_SCHEMA_OK);

    bs_value_t v = make_i32_val(42);
    bs_custom_validator_fn found;
    r = bs_custom_validator_global_find("is_positive", &found);
    assert(r == BS_SCHEMA_OK);

    char err[256];
    int result = found(&v, err, sizeof(err));
    assert(result == 1);

    bs_value_free(&v);

    /* Negative test */
    bs_value_t v2 = make_i32_val(-5);
    result = found(&v2, err, sizeof(err));
    assert(result == 0);

    bs_value_free(&v2);
    printf("OK\n");
    return 0;
}

int main(void)
{
    printf("custom_validator_test:\n");
    int fail = 0;
    fail += test_expr_arithmetic();
    fail += test_expr_logical();
    fail += test_expr_grouping();
    fail += test_expr_float();
    fail += test_cfn_global();

    printf("  %s\n", fail ? "FAILED" : "ALL PASS");
    return fail ? 1 : 0;
}
