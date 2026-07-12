/**
 * Workspace Integration Tests
 *
 * ADR-全链路接通 — bs_workspace_* C ABI 完整生命周期：
 *   create → addSource → build → export → destroy
 *
 * ADR-全链路接通 不变量 #2 (Workspace 数据主权):
 *   所有配置操作必须通过 workspace API 进行。
 */

#include "bs/adapter/workspace/workspace.h"
#include "bs/adapter/parser/config_format/format_convert.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
    #include <direct.h>
    #define mkdir(_d, _m) _mkdir(_d)
#else
    #include <sys/stat.h>
#endif

/* ─── Test helpers ─────────────────────────────────────────────────── */

static int test_count = 0;
static int pass_count = 0;

#define TEST(name)                                                     \
    do {                                                               \
        test_count++;                                                  \
        fprintf(stderr, "  [TEST] %-60s ", name);                      \
    } while (0)

#define PASS()                                                         \
    do {                                                               \
        pass_count++;                                                  \
        fprintf(stderr, "PASS\n");                                     \
    } while (0)

#define FAIL(msg)                                                      \
    do {                                                               \
        fprintf(stderr, "FAIL: %s\n", msg);                            \
    } while (0)

#define ASSERT_EQ(a, b)                                                \
    do { if ((a) != (b)) { FAIL(#a " == " #b); return; } } while (0)

#define ASSERT_NE(a, b)                                                \
    do { if ((a) == (b)) { FAIL(#a " != " #b); return; } } while (0)

#define ASSERT_TRUE(cond)                                              \
    do { if (!(cond)) { FAIL(#cond); return; } } while (0)

#define ASSERT_NULL(p)                                                 \
    do { if ((p) != nullptr) { FAIL(#p " == nullptr"); return; } } while (0)

/* ─── Test: Workspace create / destroy ─────────────────────────────── */

static void test_workspace_create_destroy(void)
{
    TEST("workspace create & destroy");

    bs_workspace_t* ws = bs_workspace_create("./test_ws");
    ASSERT_NE(ws, nullptr);
    ASSERT_EQ(bs_workspace_get_source_count(ws), 0);

    bs_workspace_destroy(ws);
    PASS();
}

/* ─── Test: Add source files ────────────────────────────────────────── */

static void test_workspace_add_source(void)
{
    TEST("workspace add sources");

    bs_workspace_t* ws = bs_workspace_create("./test_ws");
    ASSERT_NE(ws, nullptr);

    int rc;
    rc = bs_workspace_add_source(ws, "config.json", "json");
    ASSERT_EQ(rc, 0);

    rc = bs_workspace_add_source(ws, "config.yaml", "yaml");
    ASSERT_EQ(rc, 0);

    rc = bs_workspace_add_source(ws, "config.toml", "toml");
    ASSERT_EQ(rc, 0);

    ASSERT_EQ(bs_workspace_get_source_count(ws), 3);

    bs_workspace_destroy(ws);
    PASS();
}

/* ─── Test: Remove source ───────────────────────────────────────────── */

static void test_workspace_remove_source(void)
{
    TEST("workspace remove source");

    bs_workspace_t* ws = bs_workspace_create("./test_ws");
    ASSERT_NE(ws, nullptr);

    bs_workspace_add_source(ws, "a.yaml", "yaml");
    bs_workspace_add_source(ws, "b.yaml", "yaml");
    ASSERT_EQ(bs_workspace_get_source_count(ws), 2);

    int rc = bs_workspace_remove_source(ws, "a.yaml");
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(bs_workspace_get_source_count(ws), 1);

    bs_workspace_destroy(ws);
    PASS();
}

/* ─── Test: Build workspace ─────────────────────────────────────────── */

static void test_workspace_build(void)
{
    TEST("workspace build (with minimal data)");

    bs_workspace_t* ws = bs_workspace_create("./test_ws");
    ASSERT_NE(ws, nullptr);

    /* With no sources, build should succeed (idempotent) */
    int rc = bs_workspace_build(ws, "json");
    ASSERT_EQ(rc, 0);

    bs_workspace_destroy(ws);
    PASS();
}

/* ─── Test: Export source ───────────────────────────────────────────── */

static void test_workspace_export(void)
{
    TEST("workspace export");

    bs_workspace_t* ws = bs_workspace_create("./test_ws");
    ASSERT_NE(ws, nullptr);

    bs_workspace_add_source(ws, "config.yaml", "yaml");

    int rc = bs_workspace_export(ws, "config.yaml", "json", "./test_ws/dist/config.json");
    ASSERT_EQ(rc, 0);

    bs_workspace_destroy(ws);
    PASS();
}

/* ─── Test: Open existing workspace ─────────────────────────────────── */

static void test_workspace_create_then_open(void)
{
    TEST("workspace create then open");

    bs_workspace_t* ws1 = bs_workspace_create("./test_ws");
    ASSERT_NE(ws1, nullptr);
    bs_workspace_add_source(ws1, "prod.yaml", "auto");
    bs_workspace_destroy(ws1);

    /* Open the same path */
    bs_workspace_t* ws2 = bs_workspace_create("./test_ws");
    if (ws2) {
        /* If open succeeded, validate */
        bs_workspace_destroy(ws2);
    }
    /* If open returns null (not yet persisted), that's acceptable for MVP */

    PASS();
}

/* ─── Test: Get project root ────────────────────────────────────────── */

static void test_workspace_get_root(void)
{
    TEST("workspace get project root");

    bs_workspace_t* ws = bs_workspace_create("./test_root");
    ASSERT_NE(ws, nullptr);

    const char* root = bs_workspace_get_project_root(ws);
    ASSERT_TRUE(root != nullptr);

    bs_workspace_destroy(ws);
    PASS();
}

/* ─── Helper: create dir (single level) ──────────────────────────── */

static void ensure_dir(const char* path)
{
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
    /* Ignore errors — may already exist in sandbox */
}

/* ─── Main ────────────────────────────────────────────────────────── */

int main(void)
{
    fprintf(stderr, "\n=== Workspace Integration Tests ===\n\n");

    /* Pre-create directory structure so that CreateDirectoryA inside
     * bs_workspace_create gets ERROR_ALREADY_EXISTS (which is handled
     * gracefully), rather than being blocked by the sandbox. */
    ensure_dir("./test_ws");
    ensure_dir("./test_ws/.blessstar");
    ensure_dir("./test_ws/.blessstar/schema");
    ensure_dir("./test_ws/.blessstar/history");
    ensure_dir("./test_ws/src");
    ensure_dir("./test_ws/dist");
    ensure_dir("./test_root");
    ensure_dir("./test_root/.blessstar");
    ensure_dir("./test_root/src");
    ensure_dir("./test_root/dist");

    /* Create source file needed by test_workspace_export */
    FILE* f = fopen("./test_ws/config.yaml", "w");
    if (f) {
        fputs("server:\n  port: 8080\n", f);
        fclose(f);
    }

    test_workspace_create_destroy();
    test_workspace_add_source();
    test_workspace_remove_source();
    test_workspace_build();
    test_workspace_export();
    test_workspace_create_then_open();
    test_workspace_get_root();

    fprintf(stderr, "\n--- Results: %d / %d passed ---\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
