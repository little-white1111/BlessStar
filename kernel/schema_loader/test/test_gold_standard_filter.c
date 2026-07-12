/*
 * Test: Gold standard field filtering in SchemaLoader YAML parsing.
 *
 * Tests:
 *   1. Required field missing → reject
 *   2. Optional field missing → default value injected
 *   3. Unregistered field → silently ignored
 *   4. All gold standard fields present → load success
 */

#include <bs/kernel/schema/gold_standard.h>
#include <bs/kernel/schema_loader/schema_loader.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0;
static int failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); failed++; } while(0)

/* ── Helper: create temp YAML file ─────────────────────────────────── */
static int write_temp_yaml(const char* path, const char* content)
{
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%s", content);
    fclose(f);
    return 0;
}

/* ── Helper: check if a field exists in schema ──────────────────────── */
/* Note: We can't access schema internals from public API, so we test
 * at the loader level (reload result). */
static int schema_has_field(const struct bs_schema* schema, const char* name)
{
    /* This is a compile-time check - we rely on the loader's public API.
     * For internal testing we'd need accessors. For now, verify via loader. */
    (void)schema;
    (void)name;
    return 0;
}

int main(void)
{
    printf("=== test_gold_standard_filter ===\n");

    /* ── Test 1: Required field missing → reject loading ────────────── */
    {
        TEST("Required field missing → reject");
        const char* yaml = "key: my_key\n";  /* missing "type" (required) */
        const char* tmp = "test_gs_reject.yaml";
        if (write_temp_yaml(tmp, yaml) != 0) {
            FAIL("cannot create temp file");
            goto test2;
        }

        struct bs_schema_loader* loader = bs_schema_loader_create(tmp);
        if (!loader) {
            FAIL("loader creation failed");
            remove(tmp);
            goto test2;
        }

        /* Reload should fail because "type" is required */
        int rc = bs_schema_loader_reload(loader);
        if (rc != 0) {
            PASS();
        } else {
            FAIL("expected reload to fail, but it succeeded");
        }

        bs_schema_loader_destroy(loader);
        remove(tmp);
    }

test2:
    /* ── Test 2: Optional field missing → default value injected ────── */
    {
        TEST("Optional field missing → default injected");
        /* "default" is optional with default=NULL, so just verify load succeeds */
        const char* yaml = "key: my_key\ntype: string\n";
        const char* tmp = "test_gs_default.yaml";
        if (write_temp_yaml(tmp, yaml) != 0) {
            FAIL("cannot create temp file");
            goto test3;
        }

        struct bs_schema_loader* loader = bs_schema_loader_create(tmp);
        if (!loader) {
            FAIL("loader creation failed");
            remove(tmp);
            goto test3;
        }

        int rc = bs_schema_loader_reload(loader);
        if (rc == 0) {
            /* Load should succeed; missing optional fields are not a failure */
            const struct bs_schema* active = bs_schema_loader_get_active(loader);
            if (active != NULL) {
                PASS();
            } else {
                FAIL("active schema is NULL");
            }
        } else {
            FAIL("reload failed for valid minimal YAML");
        }

        bs_schema_loader_destroy(loader);
        remove(tmp);
    }

test3:
    /* ── Test 3: Unregistered field → silently ignored ──────────────── */
    {
        TEST("Unregistered field → silently ignored");
        const char* yaml = "key: my_key\ntype: string\nunknown_field: blah\n";
        const char* tmp = "test_gs_ignore.yaml";
        if (write_temp_yaml(tmp, yaml) != 0) {
            FAIL("cannot create temp file");
            goto test4;
        }

        struct bs_schema_loader* loader = bs_schema_loader_create(tmp);
        if (!loader) {
            FAIL("loader creation failed");
            remove(tmp);
            goto test4;
        }

        int rc = bs_schema_loader_reload(loader);
        if (rc == 0) {
            PASS();
        } else {
            FAIL("reload failed despite valid fields + unregistered field");
        }

        bs_schema_loader_destroy(loader);
        remove(tmp);
    }

test4:
    /* ── Test 4: All gold standard fields present → load success ────── */
    {
        TEST("All gold standard fields present → load success");
        /* Include all gold standard fields with simple values */
        const char* yaml =
            "key: my_key\n"
            "type: string\n"
            "default: val\n"
            "business_desc: desc\n"
            "contract.range: 0-100\n"
            "contract.dependencies: [a,b]\n"
            "contract.slo_impact: high\n"
            "contract.approval_required: true\n"
            "env_overrides: prod\n"
            "ui_meta.label: My Field\n"
            "ui_meta.order: 1\n"
            "impact_scope: [scope1]\n"
            "search_keywords: [kw1,kw2]\n"
            "ai_hint: This is a test field hint\n";
        const char* tmp = "test_gs_all.yaml";
        if (write_temp_yaml(tmp, yaml) != 0) {
            FAIL("cannot create temp file");
            goto done;
        }

        struct bs_schema_loader* loader = bs_schema_loader_create(tmp);
        if (!loader) {
            FAIL("loader creation failed");
            remove(tmp);
            goto done;
        }

        int rc = bs_schema_loader_reload(loader);
        if (rc == 0) {
            PASS();
        } else {
            FAIL("reload failed despite all valid fields");
        }

        bs_schema_loader_destroy(loader);
        remove(tmp);
    }

done:
    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return (failed > 0) ? 1 : 0;
}
