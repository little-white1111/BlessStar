/* Schema Registry: register / find / unregister / duplicate rejection */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bs/kernel/schema/schema_registry.h>
#include <bs/kernel/schema/schema_types.h>

static bs_schema_field_def_t make_field(const char* name, bs_schema_type_t type,
                                         int required, const char* ai_hint)
{
    bs_schema_field_def_t f;
    memset(&f, 0, sizeof(f));
    f.name     = name;
    f.type     = type;
    f.required = required ? true : false;
    f.ai_hint  = ai_hint;
    return f;
}

static int test_register_and_find(void)
{
    printf("  test_register_and_find ... ");
    bs_schema_registry_t* reg = bs_schema_registry_create();

    bs_schema_field_def_t fields[2];
    fields[0] = make_field("name", BS_SCHEMA_TYPE_STR, 1, "user name");
    fields[1] = make_field("age",  BS_SCHEMA_TYPE_I32, 0, "user age");

    bs_schema_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.schema_id  = "test.schema.1";
    entry.version    = "1.0";
    entry.root_fields = fields;
    entry.root_count  = 2;

    int r = bs_schema_register(reg, &entry);
    if (r != BS_SCHEMA_OK) { printf("FAIL: register returned %d\n", r); return 1; }

    const bs_schema_entry_t* found = bs_schema_find(reg, "test.schema.1", "1.0");
    if (!found) { printf("FAIL: not found\n"); return 1; }
    if (strcmp(found->schema_id, "test.schema.1") != 0) { printf("FAIL: id mismatch\n"); return 1; }

    bs_schema_registry_destroy(reg);
    printf("OK\n");
    return 0;
}

static int test_duplicate_rejected(void)
{
    printf("  test_duplicate_rejected ... ");
    bs_schema_registry_t* reg = bs_schema_registry_create();

    bs_schema_field_def_t f;
    memset(&f, 0, sizeof(f));
    f.name = "x"; f.type = BS_SCHEMA_TYPE_STR; f.ai_hint = "field x";

    bs_schema_entry_t e;
    memset(&e, 0, sizeof(e));
    e.schema_id = "dup"; e.version = "1.0";
    e.root_fields = &f; e.root_count = 1;

    assert(bs_schema_register(reg, &e) == BS_SCHEMA_OK);
    int r = bs_schema_register(reg, &e);
    if (r != BS_SCHEMA_ERR_ALREADY_EXISTS)
    {
        printf("FAIL: expected ALREADY_EXISTS, got %d\n", r);
        bs_schema_registry_destroy(reg);
        return 1;
    }
    bs_schema_registry_destroy(reg);
    printf("OK\n");
    return 0;
}

static int test_unregister(void)
{
    printf("  test_unregister ... ");
    bs_schema_registry_t* reg = bs_schema_registry_create();

    bs_schema_field_def_t f;
    memset(&f, 0, sizeof(f));
    f.name = "x"; f.type = BS_SCHEMA_TYPE_STR; f.ai_hint = "field x";

    bs_schema_entry_t e;
    memset(&e, 0, sizeof(e));
    e.schema_id = "del"; e.version = "1.0";
    e.root_fields = &f; e.root_count = 1;

    assert(bs_schema_register(reg, &e) == BS_SCHEMA_OK);
    assert(bs_schema_unregister(reg, "del", "1.0") == BS_SCHEMA_OK);
    assert(bs_schema_find(reg, "del", "1.0") == NULL);

    /* double delete */
    assert(bs_schema_unregister(reg, "del", "1.0") == BS_SCHEMA_ERR_NOT_FOUND);

    bs_schema_registry_destroy(reg);
    printf("OK\n");
    return 0;
}

static int test_ai_hint_rejection(void)
{
    printf("  test_ai_hint_rejection ... ");
    bs_schema_registry_t* reg = bs_schema_registry_create();

    /* Too short */
    bs_schema_field_def_t f;
    memset(&f, 0, sizeof(f));
    f.name = "x"; f.type = BS_SCHEMA_TYPE_STR; f.ai_hint = "ab"; /* < 4 */

    bs_schema_entry_t e;
    memset(&e, 0, sizeof(e));
    e.schema_id = "hint"; e.version = "1.0";
    e.root_fields = &f; e.root_count = 1;

    int r = bs_schema_register(reg, &e);
    if (r != BS_SCHEMA_ERR_AI_HINT_TOO_SHORT)
    {
        printf("FAIL: expected TOO_SHORT, got %d\n", r);
        bs_schema_registry_destroy(reg);
        return 1;
    }

    /* Missing ai_hint (null) */
    f.ai_hint = NULL;
    r = bs_schema_register(reg, &e);
    if (r != BS_SCHEMA_ERR_INVALID_ARG)
    {
        printf("FAIL: expected INVALID_ARG for null ai_hint, got %d\n", r);
        bs_schema_registry_destroy(reg);
        return 1;
    }

    bs_schema_registry_destroy(reg);
    printf("OK\n");
    return 0;
}

int main(void)
{
    printf("schema_registry_test:\n");
    int fail = 0;
    fail += test_register_and_find();
    fail += test_duplicate_rejected();
    fail += test_unregister();
    fail += test_ai_hint_rejection();

    printf("  %s\n", fail ? "FAILED" : "ALL PASS");
    return fail ? 1 : 0;
}
