/*
 * Unit tests for Schema -> UIDL converter + validator.
 *
 * Covers all schema type -> widget type mappings, nested structures,
 * ai_layout_hint passthrough, and UIDL validation.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bs/kernel/schema/schema_types.h>
#include <bs/kernel/ui_map/schema_to_uidl.h>
#include <bs/kernel/ui_map/uidl_validator.h>
#include <bs/kernel/ui_map/ui_render_desc.h>

/* ══════════════════════════════════════════════════════════════════════
 * Helper: build a field definition with minimal boilerplate
 * ══════════════════════════════════════════════════════════════════════ */
static const char* enum_role_vals[] = {"admin", "user", "guest", NULL};

static bs_schema_field_def_t make_field(const char* name, bs_schema_type_t type,
                                         int required, const char* ai_hint)
{
    bs_schema_field_def_t f;
    memset(&f, 0, sizeof(f));
    f.name     = name;
    f.type     = type;
    f.required = required ? 1 : 0;
    f.ai_hint  = ai_hint ? ai_hint : "default hint";
    return f;
}

static bs_schema_entry_t make_entry(bs_schema_field_def_t* fields,
                                     size_t count)
{
    bs_schema_entry_t e;
    memset(&e, 0, sizeof(e));
    e.schema_id   = "test.schema.v1";
    e.version     = "1.0";
    e.root_fields = fields;
    e.root_count  = count;
    e.ui_meta.title       = "Test Schema";
    e.ui_meta.description = "Test description";
    return e;
}

/* ══════════════════════════════════════════════════════════════════════
 * Test 1: String type -> input widget
 * ══════════════════════════════════════════════════════════════════════ */
static int test_string_to_input(void)
{
    printf("  test_string_to_input ... ");

    bs_schema_field_def_t f = make_field("name", BS_SCHEMA_TYPE_STR, 1, "user name");
    bs_schema_entry_t e = make_entry(&f, 1);

    char* json = NULL;
    size_t len = 0;
    int r = bs_schema_to_uidl(&e, &json, &len);
    assert(r == 0);
    assert(json != NULL);
    assert(len > 0);

    /* Verify contains expected fields */
    assert(strstr(json, "\"uidl_version\":1") != NULL);
    assert(strstr(json, "\"schema_ref\":\"test.schema.v1\"") != NULL);
    assert(strstr(json, "\"field\":\"name\"") != NULL);
    assert(strstr(json, "\"widget\":\"input\"") != NULL);
    assert(strstr(json, "\"ai_layout_hint\":\"user name\"") != NULL);

    /* Validate UIDL output */
    char** errs = NULL;
    size_t ec = 0;
    r = bs_uidl_validate(json, len, &errs, &ec);
    assert(r == 0);
    assert(ec == 0);

    free(json);
    printf("OK\n");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Test 2: Enum -> select widget
 * ══════════════════════════════════════════════════════════════════════ */
static int test_enum_to_select(void)
{
    printf("  test_enum_to_select ... ");

    bs_schema_field_def_t f = make_field("role", BS_SCHEMA_TYPE_ENUM, 1, "user role");
    f.enum_values = enum_role_vals;
    bs_schema_entry_t e = make_entry(&f, 1);

    char* json = NULL;
    size_t len = 0;
    int r = bs_schema_to_uidl(&e, &json, &len);
    assert(r == 0);
    assert(strstr(json, "\"widget\":\"select\"") != NULL);

    char** errs = NULL;
    size_t ec = 0;
    r = bs_uidl_validate(json, len, &errs, &ec);
    assert(r == 0);
    free(json);
    printf("OK\n");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Test 3: Integer types -> number widget
 * ══════════════════════════════════════════════════════════════════════ */
static int test_integer_to_number(void)
{
    printf("  test_integer_to_number ... ");

    bs_schema_field_def_t fields[3];
    fields[0] = make_field("count", BS_SCHEMA_TYPE_I32, 0, "count value");
    fields[1] = make_field("big_id", BS_SCHEMA_TYPE_I64, 0, "big id");
    fields[2] = make_field("price", BS_SCHEMA_TYPE_F64, 0, "price");
    bs_schema_entry_t e = make_entry(fields, 3);

    char* json = NULL;
    size_t len = 0;
    int r = bs_schema_to_uidl(&e, &json, &len);
    assert(r == 0);

    /* All three should map to "number" */
    assert(strstr(json, "\"widget\":\"number\"") != NULL);

    char** errs = NULL;
    size_t ec = 0;
    r = bs_uidl_validate(json, len, &errs, &ec);
    assert(r == 0);
    free(json);
    printf("OK\n");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Test 4: Bool type -> checkbox widget
 * ══════════════════════════════════════════════════════════════════════ */
static int test_bool_to_checkbox(void)
{
    printf("  test_bool_to_checkbox ... ");

    bs_schema_field_def_t f = make_field("enabled", BS_SCHEMA_TYPE_BOOL, 1, "enable flag");
    bs_schema_entry_t e = make_entry(&f, 1);

    char* json = NULL;
    size_t len = 0;
    int r = bs_schema_to_uidl(&e, &json, &len);
    assert(r == 0);
    assert(strstr(json, "\"widget\":\"checkbox\"") != NULL);

    char** errs = NULL;
    size_t ec = 0;
    r = bs_uidl_validate(json, len, &errs, &ec);
    assert(r == 0);
    free(json);
    printf("OK\n");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Test 5: Object type -> group widget + nested children
 * ══════════════════════════════════════════════════════════════════════ */
static int test_object_to_group(void)
{
    printf("  test_object_to_group ... ");

    /* Nested fields */
    bs_schema_field_def_t nested[2];
    nested[0] = make_field("street", BS_SCHEMA_TYPE_STR, 1, "street address");
    nested[1] = make_field("city", BS_SCHEMA_TYPE_STR, 1, "city name");

    bs_schema_field_def_t f = make_field("address", BS_SCHEMA_TYPE_OBJ, 0, "address object");
    f.nested_fields = nested;
    f.nested_count = 2;

    bs_schema_entry_t e = make_entry(&f, 1);

    char* json = NULL;
    size_t len = 0;
    int r = bs_schema_to_uidl(&e, &json, &len);
    assert(r == 0);
    assert(strstr(json, "\"widget\":\"group\"") != NULL);
    assert(strstr(json, "\"children\":[") != NULL);
    assert(strstr(json, "address.street") != NULL);
    assert(strstr(json, "address.city") != NULL);

    char** errs = NULL;
    size_t ec = 0;
    r = bs_uidl_validate(json, len, &errs, &ec);
    assert(r == 0);
    free(json);
    printf("OK\n");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Test 6: Array type -> repeatable_group widget
 * ══════════════════════════════════════════════════════════════════════ */
static int test_array_to_repeatable(void)
{
    printf("  test_array_to_repeatable ... ");

    bs_schema_field_def_t f = make_field("tags", BS_SCHEMA_TYPE_ARR, 0, "string tags");
    f.elem_type = BS_SCHEMA_TYPE_STR;
    bs_schema_entry_t e = make_entry(&f, 1);

    char* json = NULL;
    size_t len = 0;
    int r = bs_schema_to_uidl(&e, &json, &len);
    assert(r == 0);
    assert(strstr(json, "\"widget\":\"repeatable_group\"") != NULL);

    char** errs = NULL;
    size_t ec = 0;
    r = bs_uidl_validate(json, len, &errs, &ec);
    assert(r == 0);
    free(json);
    printf("OK\n");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Test 7: Array of objects -> repeatable_group with children
 * ══════════════════════════════════════════════════════════════════════ */
static int test_array_of_objects(void)
{
    printf("  test_array_of_objects ... ");

    bs_schema_field_def_t elem_f[2];
    elem_f[0] = make_field("key", BS_SCHEMA_TYPE_STR, 1, "entry key");
    elem_f[1] = make_field("value", BS_SCHEMA_TYPE_STR, 1, "entry value");

    bs_schema_field_def_t f = make_field("entries", BS_SCHEMA_TYPE_ARR, 0, "key-value entries");
    f.elem_type = BS_SCHEMA_TYPE_OBJ;
    f.elem_fields = elem_f;
    f.elem_nested_count = 2;
    bs_schema_entry_t e = make_entry(&f, 1);

    char* json = NULL;
    size_t len = 0;
    int r = bs_schema_to_uidl(&e, &json, &len);
    assert(r == 0);
    /* Array -> repeatable_group */
    assert(strstr(json, "\"widget\":\"repeatable_group\"") != NULL);
    /* Children are the element fields */
    assert(strstr(json, "\"children\":[") != NULL);

    char** errs = NULL;
    size_t ec = 0;
    r = bs_uidl_validate(json, len, &errs, &ec);
    assert(r == 0);
    free(json);
    printf("OK\n");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Test 8: Mixed fields — all type mappings in one schema
 * ══════════════════════════════════════════════════════════════════════ */
static int test_mixed_fields(void)
{
    printf("  test_mixed_fields ... ");

    bs_schema_field_def_t fields[6];
    fields[0] = make_field("name", BS_SCHEMA_TYPE_STR, 1, "name field");
    fields[1] = make_field("role", BS_SCHEMA_TYPE_ENUM, 1, "role field");
    fields[1].enum_values = enum_role_vals;
    fields[2] = make_field("age", BS_SCHEMA_TYPE_I32, 0, "age field");
    fields[3] = make_field("active", BS_SCHEMA_TYPE_BOOL, 0, "active flag");
    fields[4] = make_field("tags", BS_SCHEMA_TYPE_ARR, 0, "tags array");
    fields[4].elem_type = BS_SCHEMA_TYPE_STR;
    fields[5].name = "settings";
    fields[5].type = BS_SCHEMA_TYPE_OBJ;
    fields[5].required = 0;
    fields[5].ai_hint = "settings object";
    fields[5].nested_fields = NULL;
    fields[5].nested_count = 0;

    bs_schema_entry_t e = make_entry(fields, 6);

    char* json = NULL;
    size_t len = 0;
    int r = bs_schema_to_uidl(&e, &json, &len);
    assert(r == 0);

    /* Check all widget types present */
    assert(strstr(json, "\"widget\":\"input\"") != NULL);
    assert(strstr(json, "\"widget\":\"select\"") != NULL);
    assert(strstr(json, "\"widget\":\"number\"") != NULL);
    assert(strstr(json, "\"widget\":\"checkbox\"") != NULL);
    assert(strstr(json, "\"widget\":\"repeatable_group\"") != NULL);
    assert(strstr(json, "\"widget\":\"group\"") != NULL);

    /* Validate */
    char** errs = NULL;
    size_t ec = 0;
    r = bs_uidl_validate(json, len, &errs, &ec);
    assert(r == 0);
    free(json);
    printf("OK\n");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Test 9: ai_layout_hint passthrough (UIDL-02)
 * ══════════════════════════════════════════════════════════════════════ */
static int test_ai_layout_hint_passthrough(void)
{
    printf("  test_ai_layout_hint_passthrough ... ");

    bs_schema_field_def_t f = make_field("api_key", BS_SCHEMA_TYPE_STR, 1,
                                           "sensitive field; mask in UI");
    bs_schema_entry_t e = make_entry(&f, 1);

    char* json = NULL;
    size_t len = 0;
    int r = bs_schema_to_uidl(&e, &json, &len);
    assert(r == 0);

    /* ai_layout_hint should contain the ai_hint text verbatim */
    assert(strstr(json, "\"ai_layout_hint\":\"sensitive field; mask in UI\"") != NULL);

    free(json);
    printf("OK\n");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Test 10: ui_order passthrough
 * ══════════════════════════════════════════════════════════════════════ */
static int test_order_passthrough(void)
{
    printf("  test_order_passthrough ... ");

    bs_schema_field_def_t f = make_field("z_last", BS_SCHEMA_TYPE_STR, 0, "last field");
    f.ui_order = 10;
    bs_schema_entry_t e = make_entry(&f, 1);

    char* json = NULL;
    size_t len = 0;
    int r = bs_schema_to_uidl(&e, &json, &len);
    assert(r == 0);
    assert(strstr(json, "\"order\":10") != NULL);

    free(json);
    printf("OK\n");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Test 11: visibility is always null (UIDL-03)
 * ══════════════════════════════════════════════════════════════════════ */
static int test_visibility_null(void)
{
    printf("  test_visibility_null ... ");

    bs_schema_field_def_t f = make_field("hidden_field", BS_SCHEMA_TYPE_STR, 0, "conditionally shown");
    bs_schema_entry_t e = make_entry(&f, 1);

    char* json = NULL;
    size_t len = 0;
    int r = bs_schema_to_uidl(&e, &json, &len);
    assert(r == 0);
    assert(strstr(json, "\"visibility\":null") != NULL);

    free(json);
    printf("OK\n");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Test 12: String + enum -> select (not input)
 * ══════════════════════════════════════════════════════════════════════ */
static int test_string_with_enum(void)
{
    printf("  test_string_with_enum ... ");

    bs_schema_field_def_t f = make_field("color", BS_SCHEMA_TYPE_STR, 1, "color choice");
    const char* colors[] = {"red", "green", "blue", NULL};
    f.enum_values = colors;
    bs_schema_entry_t e = make_entry(&f, 1);

    char* json = NULL;
    size_t len = 0;
    int r = bs_schema_to_uidl(&e, &json, &len);
    assert(r == 0);
    /* STR with enum -> select */
    assert(strstr(json, "\"widget\":\"select\"") != NULL);

    free(json);
    printf("OK\n");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Test 13: Validate invalid UIDL — missing required field
 * ══════════════════════════════════════════════════════════════════════ */
static int test_validate_missing_field(void)
{
    printf("  test_validate_missing_field ... ");

    const char* bad_json = "{\"uidl_version\":1,\"schema_ref\":\"test\"}";
    char** errs = NULL;
    size_t ec = 0;
    int r = bs_uidl_validate(bad_json, strlen(bad_json), &errs, &ec);
    assert(r > 0); /* validation error */
    assert(ec > 0);
    bs_uidl_validate_errors_free(errs, ec);
    printf("OK\n");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Test 14: Validate invalid UIDL — unknown widget
 * ══════════════════════════════════════════════════════════════════════ */
static int test_validate_unknown_widget(void)
{
    printf("  test_validate_unknown_widget ... ");

    const char* bad_json =
        "{\"uidl_version\":1,\"schema_ref\":\"t\","
        "\"controls\":[{\"field\":\"x\",\"widget\":\"not_a_widget\","
        "\"order\":0,\"children\":[],\"visibility\":null,"
        "\"default_value\":null,\"validation_ref\":null}]}";
    char** errs = NULL;
    size_t ec = 0;
    int r = bs_uidl_validate(bad_json, strlen(bad_json), &errs, &ec);
    assert(r > 0);
    assert(ec > 0);
    bs_uidl_validate_errors_free(errs, ec);
    printf("OK\n");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Main
 * ══════════════════════════════════════════════════════════════════════ */
int main(void)
{
    printf("schema_to_uidl_test:\n");
    int fail = 0;

    fail += test_string_to_input();
    fail += test_enum_to_select();
    fail += test_integer_to_number();
    fail += test_bool_to_checkbox();
    fail += test_object_to_group();
    fail += test_array_to_repeatable();
    fail += test_array_of_objects();
    fail += test_mixed_fields();
    fail += test_ai_layout_hint_passthrough();
    fail += test_order_passthrough();
    fail += test_visibility_null();
    fail += test_string_with_enum();
    fail += test_validate_missing_field();
    fail += test_validate_unknown_widget();

    printf("  %s\n", fail ? "FAILED" : "ALL PASS");
    return fail ? 1 : 0;
}
