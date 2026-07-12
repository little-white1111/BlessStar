/**
 * LocalDirectStorage Unit Tests.
 *
 * ADR-workspace加固 测试要求：
 *   单元测试: LocalDirectStorage 4 个方法（read/write/exists/remove）
 *   路径穿越测试: read("../../etc/passwd") 返回 -EINVAL
 *   双后端验证: BSTORAGE=local ctest -R storage
 */

#include "bs/adapter/workspace/storage_local.h"
#include "bs/adapter/workspace/storage_backend.h"

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

/* ─── Test framework ─────────────────────────────────────────────────── */

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

#define ASSERT_LT(a, b)                                                \
    do { if (!((a) < (b))) { FAIL(#a " < " #b); return; } } while (0)

/* ─── Test helpers ─────────────────────────────────────────────────── */

static void ensure_dir(const char* path)
{
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

/* Clean up all files in test directory then remove it */
static void cleanup_test_dir(const char* path)
{
    /* Delete known files created by tests */
    const char* files[] = {
        "test.txt", "exists.txt", "remove_me.txt",
        "empty.txt", "no_such_file.yaml", NULL
    };
    char full[512];
    for (int i = 0; files[i]; ++i) {
        snprintf(full, sizeof(full), "%s/%s", path, files[i]);
        remove(full);
    }
#ifdef _WIN32
    _rmdir(path);
#else
    rmdir(path);
#endif
}

/* ─── Test: LocalDirectStorage write + read ───────────────────────── */

static void test_storage_write_read(void)
{
    TEST("storage write then read");

    StorageBackend* sb = LocalDirectStorage_new("./test_storage");
    ASSERT_NE(sb, nullptr);

    const char* test_data = "hello workspace storage";
    size_t test_len = strlen(test_data) + 1;

    int rc = sb->write(sb, "test.txt",
                        (const uint8_t*)test_data, test_len);
    ASSERT_EQ(rc, 0);

    uint8_t* buf = NULL;
    size_t   buf_len = 0;
    rc = sb->read(sb, "test.txt", &buf, &buf_len);
    ASSERT_EQ(rc, 0);
    ASSERT_NE(buf, nullptr);
    ASSERT_EQ(buf_len, test_len);
    ASSERT_EQ(memcmp(buf, test_data, test_len), 0);

    free(buf);
    sb->destroy(sb);
    delete sb;
    PASS();
}

/* ─── Test: LocalDirectStorage exists ──────────────────────────────── */

static void test_storage_exists(void)
{
    TEST("storage exists / not-exists");

    StorageBackend* sb = LocalDirectStorage_new("./test_storage");
    ASSERT_NE(sb, nullptr);

    /* File should not exist yet */
    int rc = sb->exists(sb, "nonexistent.txt");
    ASSERT_EQ(rc, 0);

    /* Create file via write */
    const char* data = "exists";
    sb->write(sb, "exists.txt", (const uint8_t*)data, strlen(data) + 1);

    /* Should exist now */
    rc = sb->exists(sb, "exists.txt");
    ASSERT_EQ(rc, 1);

    sb->destroy(sb);
    delete sb;
    PASS();
}

/* ─── Test: LocalDirectStorage remove ──────────────────────────────── */

static void test_storage_remove(void)
{
    TEST("storage remove");

    StorageBackend* sb = LocalDirectStorage_new("./test_storage");
    ASSERT_NE(sb, nullptr);

    const char* data = "to be removed";
    sb->write(sb, "remove_me.txt", (const uint8_t*)data, strlen(data) + 1);

    int rc = sb->remove_fn(sb, "remove_me.txt");
    ASSERT_EQ(rc, 0);

    /* Should no longer exist */
    rc = sb->exists(sb, "remove_me.txt");
    ASSERT_EQ(rc, 0);

    /* Remove non-existent should return -ENOENT */
    rc = sb->remove_fn(sb, "already_gone.txt");
    ASSERT_EQ(rc, -ENOENT);

    sb->destroy(sb);
    delete sb;
    PASS();
}

/* ─── Test: Path traversal rejection ───────────────────────────────── */

static void test_storage_path_traversal(void)
{
    TEST("storage path traversal rejected");

    StorageBackend* sb = LocalDirectStorage_new("./test_storage");
    ASSERT_NE(sb, nullptr);

    uint8_t* buf = NULL;
    size_t   len = 0;

    int rc = sb->read(sb, "../../etc/passwd", &buf, &len);
    ASSERT_EQ(rc, -EINVAL);
    ASSERT_EQ(buf, nullptr);

    rc = sb->read(sb, "subdir/../../etc/passwd", &buf, &len);
    ASSERT_EQ(rc, -EINVAL);

    rc = sb->write(sb, "../outside.txt", (const uint8_t*)"x", 1);
    ASSERT_EQ(rc, -EINVAL);

    rc = sb->exists(sb, "..\\windows\\system32\\config");
    ASSERT_EQ(rc, -EINVAL);

    rc = sb->remove_fn(sb, "safe/../../evil.txt");
    ASSERT_EQ(rc, -EINVAL);

    sb->destroy(sb);
    delete sb;
    PASS();
}

/* ─── Test: storage_path_safe unit ─────────────────────────────────── */

static void test_path_safe(void)
{
    TEST("storage_path_safe validation");

    /* Safe paths */
    ASSERT_EQ(storage_path_safe("config.yaml"), 0);
    ASSERT_EQ(storage_path_safe("subdir/config.json"), 0);
    ASSERT_EQ(storage_path_safe("a.b/c.d/e.f"), 0);
    ASSERT_EQ(storage_path_safe(".dotfile"), 0);
    ASSERT_EQ(storage_path_safe("dots.in.name"), 0);

    /* Unsafe paths */
    ASSERT_LT(storage_path_safe("../etc/passwd"), 0);
    ASSERT_LT(storage_path_safe("../../etc/passwd"), 0);
    ASSERT_LT(storage_path_safe("a/../../b"), 0);
    ASSERT_LT(storage_path_safe("a\\..\\b"), 0);

    /* Edge cases */
    ASSERT_LT(storage_path_safe(".."), 0);
    ASSERT_LT(storage_path_safe("../.."), 0);
    ASSERT_LT(storage_path_safe("a/.."), 0);

    /* Null */
    ASSERT_LT(storage_path_safe(NULL), 0);

    PASS();
}

/* ─── Test: write empty content ────────────────────────────────────── */

static void test_storage_write_empty(void)
{
    TEST("storage write empty content");

    StorageBackend* sb = LocalDirectStorage_new("./test_storage");
    ASSERT_NE(sb, nullptr);

    int rc = sb->write(sb, "empty.txt", (const uint8_t*)"", 0);
    ASSERT_EQ(rc, 0);

    uint8_t* buf = NULL;
    size_t   len = 0;
    rc = sb->read(sb, "empty.txt", &buf, &len);
    ASSERT_EQ(rc, 0);
    ASSERT_NE(buf, nullptr);
    ASSERT_EQ(len, 1);  /* null terminator */
    ASSERT_EQ(buf[0], '\0');

    free(buf);
    sb->destroy(sb);
    delete sb;
    PASS();
}

/* ─── Test: storage read non-existent file ──────────────────────────── */

static void test_storage_read_nonexistent(void)
{
    TEST("storage read non-existent returns -ENOENT");

    StorageBackend* sb = LocalDirectStorage_new("./test_storage");
    ASSERT_NE(sb, nullptr);

    uint8_t* buf = NULL;
    size_t   len = 0;
    int rc = sb->read(sb, "no_such_file.yaml", &buf, &len);
    ASSERT_EQ(rc, -ENOENT);
    ASSERT_EQ(buf, nullptr);

    sb->destroy(sb);
    delete sb;
    PASS();
}

/* ─── Main ────────────────────────────────────────────────────────── */

int main(void)
{
    fprintf(stderr, "\n=== LocalDirectStorage Unit Tests ===\n\n");

    ensure_dir("./test_storage");

    test_path_safe();
    test_storage_write_read();
    test_storage_exists();
    test_storage_remove();
    test_storage_path_traversal();
    test_storage_write_empty();
    test_storage_read_nonexistent();

    /* Cleanup test directory */
    cleanup_test_dir("./test_storage");

    fprintf(stderr, "\n--- Results: %d / %d passed ---\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
