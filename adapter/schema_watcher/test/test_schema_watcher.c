/*
 * Test: SchemaWatcher lifecycle and mtime polling (non-blocking).
 *
 * Tests:
 *   1. Create/destroy lifecycle
 *   2. check_and_reload detects mtime change
 *   3. check_and_reload returns 0 when no change
 */

#include <bs/adapter/schema_watcher/schema_watcher.h>
#include <bs/kernel/schema_loader/schema_loader.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Windows compatibility: use _stat / _sleep */
#ifdef _MSC_VER
#include <sys/stat.h>
#define bs_stat _stat
#else
#include <sys/stat.h>
#define bs_stat stat
#endif

#ifdef _WIN32
#include <windows.h>
#define bs_sleep_ms(ms) Sleep((DWORD)(ms))
#else
#include <unistd.h>
#define bs_sleep_ms(ms) usleep((useconds_t)(ms) * 1000)
#endif

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

/* ── Switch callback ───────────────────────────────────────────────── */
static int g_reload_count = 0;
static void reload_cb(const struct bs_schema* new_schema, void* userdata)
{
    (void)new_schema;
    (void)userdata;
    g_reload_count++;
}

int main(void)
{
    printf("=== test_schema_watcher ===\n");

    /* ── Test 1: Create/destroy lifecycle ───────────────────────────── */
    {
        TEST("Create/destroy lifecycle");
        const char* yaml = "key: test_key\ntype: string\n";
        const char* tmp = "test_watcher_lifecycle.yaml";
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

        struct bs_schema_watcher* watcher = bs_schema_watcher_create(tmp, loader);
        if (!watcher) {
            FAIL("schema_watcher creation failed");
            bs_schema_loader_destroy(loader);
            remove(tmp);
            goto test2;
        }

        /* Just verify we can destroy without crash */
        bs_schema_watcher_destroy(watcher);
        bs_schema_loader_destroy(loader);
        remove(tmp);
        PASS();
    }

test2:
    /* ── Test 2: check_and_reload detects mtime change ──────────────── */
    {
        TEST("check_and_reload detects mtime change -> returns 1");
        const char* yaml = "key: original_key\ntype: string\n";
        const char* tmp = "test_watcher_mtime.yaml";
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

        g_reload_count = 0;
        bs_schema_loader_on_switch(loader, reload_cb, NULL);

        struct bs_schema_watcher* watcher = bs_schema_watcher_create(tmp, loader);
        if (!watcher) {
            FAIL("schema_watcher creation failed");
            bs_schema_loader_destroy(loader);
            remove(tmp);
            goto test3;
        }

        /* Modify the YAML file to trigger mtime change */
        bs_sleep_ms(100);
        const char* yaml2 = "key: modified_key\ntype: string\n";
        if (write_temp_yaml(tmp, yaml2) != 0) {
            FAIL("cannot write modified yaml");
            bs_schema_watcher_destroy(watcher);
            bs_schema_loader_destroy(loader);
            remove(tmp);
            goto test3;
        }

        /* Call non-blocking check_and_reload — should detect the change */
        int rc = bs_schema_watcher_check_and_reload(watcher);

        bs_schema_watcher_destroy(watcher);
        bs_schema_loader_destroy(loader);
        remove(tmp);

        if (rc == 1) {
            PASS();
        } else {
            FAIL(rc == 0 ? "no change detected" : "error during check_and_reload");
        }
    }

test3:
    /* ── Test 3: check_and_reload returns 0 when no change ──────────── */
    {
        TEST("check_and_reload returns 0 when no change");
        const char* yaml = "key: stable_key\ntype: string\n";
        const char* tmp = "test_watcher_stable.yaml";
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

        struct bs_schema_watcher* watcher = bs_schema_watcher_create(tmp, loader);
        if (!watcher) {
            FAIL("schema_watcher creation failed");
            bs_schema_loader_destroy(loader);
            remove(tmp);
            goto done;
        }

        /* First call initializes mtime, second call should report no change */
        int rc1 = bs_schema_watcher_check_and_reload(watcher);
        int rc2 = bs_schema_watcher_check_and_reload(watcher);

        bs_schema_watcher_destroy(watcher);
        bs_schema_loader_destroy(loader);
        remove(tmp);

        if (rc1 == 0 && rc2 == 0) {
            PASS();
        } else {
            FAIL("expected 0 (no change) on both calls");
        }
    }

done:
    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return (failed > 0) ? 1 : 0;
}
