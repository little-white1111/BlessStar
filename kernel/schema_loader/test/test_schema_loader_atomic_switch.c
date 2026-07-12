/*
 * Test: Atomic switch of SchemaLoader (three-phase reload).
 *
 * Tests:
 *   1. Normal load (valid YAML)
 *   2. Validation failure → rollback to old schema
 *   3. Empty ruleset fallback (YAML not found)
 */

#include <bs/kernel/schema_loader/schema_loader.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0;
static int failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)

static int write_temp_yaml(const char* path, const char* content)
{
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%s", content);
    fclose(f);
    return 0;
}

/* ── Switch callback counter ───────────────────────────────────────── */
static int g_switch_count = 0;
static void switch_cb(const struct bs_schema* new_schema, void* userdata)
{
    (void)new_schema;
    (void)userdata;
    g_switch_count++;
}

int main(void)
{
    printf("=== test_schema_loader_atomic_switch ===\n");

    /* ── Test 1: Normal load ────────────────────────────────────────── */
    {
        TEST("Normal load");
        const char* yaml = "key: test_key\ntype: int\n";
        const char* tmp = "test_switch_normal.yaml";
        if (write_temp_yaml(tmp, yaml) != 0) {
            FAIL("cannot create temp file");
            goto test2;
        }

        struct bs_schema_loader* loader = bs_schema_loader_create(tmp);
        if (!loader) {
            FAIL("loader creation failed");
            remove(tmp);
            goto test3;
        }

        int rc = bs_schema_loader_reload(loader);
        if (rc == 0) {
            const struct bs_schema* active = bs_schema_loader_get_active(loader);
            if (active != NULL) {
                PASS();
            } else {
                FAIL("active schema is NULL after successful reload");
            }
        } else {
            FAIL("reload returned error");
        }

        bs_schema_loader_destroy(loader);
        remove(tmp);
    }

test2:
    /* ── Test 2: Validation failure → rollback ──────────────────────── */
    {
        TEST("Validation failure → rollback");
        const char* yaml1 = "key: good_key\ntype: string\n";
        const char* yaml2 = "key: bad_key\n";  /* missing "type" (required) */
        const char* tmp = "test_switch_rollback.yaml";

        /* First load a valid schema */
        if (write_temp_yaml(tmp, yaml1) != 0) {
            FAIL("cannot create temp file (yaml1)");
            goto test3;
        }

        struct bs_schema_loader* loader = bs_schema_loader_create(tmp);
        if (!loader) {
            FAIL("loader creation failed");
            remove(tmp);
            goto test3;
        }

        g_switch_count = 0;
        bs_schema_loader_on_switch(loader, switch_cb, NULL);

        /* Initial reload should succeed */
        int rc = bs_schema_loader_reload(loader);
        if (rc != 0) {
            FAIL("initial reload failed");
            bs_schema_loader_destroy(loader);
            remove(tmp);
            goto test3;
        }

        /* Now write invalid YAML and try reload */
        if (write_temp_yaml(tmp, yaml2) != 0) {
            FAIL("cannot write yaml2");
            bs_schema_loader_destroy(loader);
            remove(tmp);
            goto test3;
        }

        int switch_before = g_switch_count;
        rc = bs_schema_loader_reload(loader);
        if (rc != 0) {
            /* Reload failed; old schema should be preserved */
            const struct bs_schema* active = bs_schema_loader_get_active(loader);
            if (active != NULL && g_switch_count == switch_before) {
                PASS();
            } else {
                FAIL("old schema not preserved or callback fired incorrectly");
            }
        } else {
            FAIL("reload should have failed but succeeded");
        }

        bs_schema_loader_destroy(loader);
        remove(tmp);
    }

test3:
    /* ── Test 3: Empty ruleset (YAML not found) ─────────────────────── */
    {
        TEST("Empty ruleset fallback (YAML not found)");
        const char* nonexistent = "test_switch_nonexistent.yaml";

        /* Ensure file doesn't exist */
        remove(nonexistent);

        struct bs_schema_loader* loader = bs_schema_loader_create(nonexistent);
        if (!loader) {
            FAIL("loader creation failed");
            goto done;
        }

        /* When YAML doesn't exist, loader should start with empty ruleset */
        const struct bs_schema* active = bs_schema_loader_get_active(loader);
        if (active == NULL) {
            /* NULL is valid for empty ruleset */
            PASS();
        } else {
            FAIL("expected NULL active schema for nonexistent YAML");
        }

        bs_schema_loader_destroy(loader);
    }

done:
    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return (failed > 0) ? 1 : 0;
}
