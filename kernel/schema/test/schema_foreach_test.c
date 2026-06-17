/* OPT-01: Schema Registry foreach / count tests */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bs/kernel/schema/schema_registry.h>
#include <bs/kernel/schema/schema_types.h>

static int foreach_count = 0;

static void foreach_cb(const bs_schema_entry_t* entry, void* userdata)
{
    (void)entry;
    int* cnt = (int*)userdata;
    (*cnt)++;
    foreach_count++;
}

static int test_foreach_empty(void)
{
    printf("  test_foreach_empty ... ");
    bs_schema_registry_t* reg = bs_schema_registry_create();

    int cnt = 0;
    int r = bs_schema_foreach(reg, foreach_cb, &cnt);
    if (r != BS_SCHEMA_OK) { printf("FAIL: foreach returned %d\n", r); return 1; }
    if (cnt != 0) { printf("FAIL: expected 0, got %d\n", cnt); return 1; }

    size_t c = bs_schema_count(reg);
    if (c != 0) { printf("FAIL: count expected 0, got %zu\n", c); return 1; }

    bs_schema_registry_destroy(reg);
    printf("OK\n");
    return 0;
}

static int test_foreach_multiple(void)
{
    printf("  test_foreach_multiple ... ");
    bs_schema_registry_t* reg = bs_schema_registry_create();

    bs_schema_field_def_t f1, f2;
    memset(&f1, 0, sizeof(f1)); f1.name = "x"; f1.type = BS_SCHEMA_TYPE_STR; f1.ai_hint = "field x hint";
    memset(&f2, 0, sizeof(f2)); f2.name = "y"; f2.type = BS_SCHEMA_TYPE_I32; f2.ai_hint = "field y hint";

    bs_schema_entry_t e1, e2;
    memset(&e1, 0, sizeof(e1)); e1.schema_id = "test.a"; e1.version = "1.0"; e1.root_fields = &f1; e1.root_count = 1;
    memset(&e2, 0, sizeof(e2)); e2.schema_id = "test.b"; e2.version = "1.0"; e2.root_fields = &f2; e2.root_count = 1;

    assert(bs_schema_register(reg, &e1) == BS_SCHEMA_OK);
    assert(bs_schema_register(reg, &e2) == BS_SCHEMA_OK);

    size_t c = bs_schema_count(reg);
    if (c != 2) { printf("FAIL: count expected 2, got %zu\n", c); return 1; }

    int cnt = 0;
    foreach_count = 0;
    int r = bs_schema_foreach(reg, foreach_cb, &cnt);
    if (r != BS_SCHEMA_OK) { printf("FAIL: foreach returned %d\n", r); return 1; }
    if (cnt != 2) { printf("FAIL: foreach count expected 2, got %d\n", cnt); return 1; }
    if (foreach_count != 2) { printf("FAIL: static foreach_count expected 2, got %d\n", foreach_count); return 1; }

    bs_schema_registry_destroy(reg);
    printf("OK\n");
    return 0;
}

static int test_foreach_null_check(void)
{
    printf("  test_foreach_null_check ... ");
    int r = bs_schema_foreach(NULL, foreach_cb, NULL);
    if (r != BS_SCHEMA_ERR_INVALID_ARG) { printf("FAIL: expected INVALID_ARG, got %d\n", r); return 1; }

    bs_schema_registry_t* reg = bs_schema_registry_create();
    r = bs_schema_foreach(reg, NULL, NULL);
    if (r != BS_SCHEMA_ERR_INVALID_ARG) { printf("FAIL: expected INVALID_ARG, got %d\n", r); return 1; }

    size_t c = bs_schema_count(NULL);
    if (c != 0) { printf("FAIL: count on NULL expected 0, got %zu\n", c); return 1; }

    bs_schema_registry_destroy(reg);
    printf("OK\n");
    return 0;
}

int main(void)
{
    printf("schema_foreach_test:\n");
    int fail = 0;
    fail += test_foreach_empty();
    fail += test_foreach_multiple();
    fail += test_foreach_null_check();
    printf("  %s\n", fail ? "FAILED" : "ALL PASS");
    return fail ? 1 : 0;
}
