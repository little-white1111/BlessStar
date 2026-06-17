/* Round-trip converter test */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bs/kernel/schema/schema_json_converter.h>
#include <bs/kernel/schema/schema_types.h>

static bs_schema_field_def_t fields[3];
static const char* enum_vals_static[] = {"admin", "user", NULL};

static void setup_fields(void)
{
    memset(fields, 0, sizeof(fields));

    fields[0].name     = "name";
    fields[0].type     = BS_SCHEMA_TYPE_STR;
    fields[0].required = true;
    fields[0].pattern  = "^.{1,64}$";
    fields[0].ai_hint  = "user name between 1-64 chars";
    fields[0].ui_label = "Name";

    fields[1].name     = "age";
    fields[1].type     = BS_SCHEMA_TYPE_I32;
    fields[1].required = false;
    fields[1].range.has_min = true; fields[1].range.min = 0;
    fields[1].range.has_max = true; fields[1].range.max = 150;
    fields[1].ai_hint  = "user age 0-150";

    fields[2].name     = "role";
    fields[2].type     = BS_SCHEMA_TYPE_ENUM;
    fields[2].required = true;
    fields[2].enum_values = enum_vals_static;
    fields[2].ai_hint  = "user role admin/user";
}

static int test_roundtrip(void)
{
    printf("  test_roundtrip ... ");

    setup_fields();

    bs_schema_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.schema_id   = "com.example.test";
    entry.version     = "1.0";
    entry.root_fields = fields;
    entry.root_count  = 3;
    entry.ui_meta.title = "Test Schema";
    entry.ui_meta.description = "Round-trip test";

    char* json_out = NULL;
    size_t json_len = 0;
    int r = bs_json_converter_to_draft07(&entry, &json_out, &json_len);
    if (r != BS_SCHEMA_OK) { printf("FAIL: to_draft07 returned %d\n", r); return 1; }
    if (!json_out || json_len == 0) { printf("FAIL: empty output\n"); free(json_out); return 1; }

    /* Convert back */
    bs_schema_entry_t* back = NULL;
    r = bs_json_converter_from_draft07(json_out, json_len, &back);
    if (r != BS_SCHEMA_OK) { printf("FAIL: from_draft07 returned %d\n", r); free(json_out); return 1; }

    /* Verify */
    if (!back) { printf("FAIL: null entry\n"); free(json_out); return 1; }

    /* Check field count */
    if (back->root_count != 3)
    {
        printf("FAIL: expected 3 fields, got %zu\n", back->root_count);
        bs_json_converter_free_entry(back); free(back); free(json_out);
        return 1;
    }

    /* Check name field */
    if (strcmp(back->root_fields[0].name, "name") != 0)
    {
        printf("FAIL: first field name mismatch\n");
        bs_json_converter_free_entry(back); free(back); free(json_out);
        return 1;
    }

    /* Check ai_hint present */
    if (!back->root_fields[0].ai_hint || strlen(back->root_fields[0].ai_hint) == 0)
    {
        printf("FAIL: ai_hint missing in round-tripped field\n");
        bs_json_converter_free_entry(back); free(back); free(json_out);
        return 1;
    }

    /* Check type */
    if (back->root_fields[0].type != BS_SCHEMA_TYPE_STR)
    {
        printf("FAIL: type mismatch for 'name'\n");
        bs_json_converter_free_entry(back); free(back); free(json_out);
        return 1;
    }

    bs_json_converter_free_entry(back);
    free(back);
    free(json_out);
    printf("OK\n");
    return 0;
}

static int test_roundtrip_exact(void)
{
    printf("  test_roundtrip_exact ... ");

    setup_fields();

    bs_schema_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.schema_id   = "exact.test";
    entry.version     = "1.0";
    entry.root_fields = fields;
    entry.root_count  = 3;

    char* json_out = NULL;
    size_t json_len = 0;
    int r = bs_json_converter_to_draft07(&entry, &json_out, &json_len);
    assert(r == BS_SCHEMA_OK);

    bs_schema_entry_t* back = NULL;
    r = bs_json_converter_from_draft07(json_out, json_len, &back);
    assert(r == BS_SCHEMA_OK);

    /* Check required preserved */
    assert(back->root_fields[0].required == true);  /* name */
    assert(back->root_fields[1].required == false); /* age */
    assert(back->root_fields[2].required == true);  /* role */

    bs_json_converter_free_entry(back);
    free(back);
    free(json_out);
    printf("OK\n");
    return 0;
}

int main(void)
{
    printf("converter_roundtrip_test:\n");
    int fail = 0;
    fail += test_roundtrip();
    fail += test_roundtrip_exact();

    printf("  %s\n", fail ? "FAILED" : "ALL PASS");
    return fail ? 1 : 0;
}
