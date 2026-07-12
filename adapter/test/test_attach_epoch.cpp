/**
 * Epoch Version Manager Unit Tests.
 *
 * 持久化与通信修复 (方案C，Fix 3):
 * 引入 epoch 版本号，串联三层（WAL → configs.json → workspace），
 * 写入时验证 incoming_epoch == current_epoch + 1（CAS 语义）。
 *
 * 测试覆盖:
 *   1) bs_epoch_init 初始化全为 0
 *   2) bs_epoch_next 单调递增
 *   3) bs_epoch_validate 接受/拒绝 (含 stale/跳跃)
 *   4) serialize/deserialize 往返
 *   5) persist/load 往返 (临时文件)
 */

#include "bs/adapter/persistence/attach_epoch.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
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

#define ASSERT_TRUE(c)                                                 \
    do { if (!(c)) { FAIL(#c); return; } } while (0)

#define ASSERT_FALSE(c)                                                 \
    do { if ((c)) { FAIL("!" #c); return; } } while (0)

/* ─── Helpers ────────────────────────────────────────────────────────── */

static std::string temp_file_path() {
    const char* tmpdir = nullptr;
#ifdef _WIN32
    tmpdir = getenv("TEMP");
    if (!tmpdir) tmpdir = "C:\\Windows\\Temp";
#else
    tmpdir = "/tmp";
#endif
    return std::string(tmpdir) + "/bs_epoch_test_" +
           std::to_string(rand() % 1000000) + ".json";
}

/* ─── Test cases ─────────────────────────────────────────────────────── */

static void test_init() {
    TEST("bs_epoch_init 初始化全为 0");
    bs_epoch_state_t epoch;
    bs_epoch_init(&epoch);
    ASSERT_EQ(epoch.current_epoch, 0ULL);
    ASSERT_EQ(epoch.wal_epoch, 0ULL);
    ASSERT_EQ(epoch.cache_epoch, 0ULL);
    PASS();
}

static void test_next_once() {
    TEST("bs_epoch_next 单调递增 0→1");
    bs_epoch_state_t epoch;
    bs_epoch_init(&epoch);
    uint64_t next = bs_epoch_next(&epoch);
    ASSERT_EQ(next, 1ULL);
    ASSERT_EQ(epoch.current_epoch, 1ULL);
    PASS();
}

static void test_next_monotonic() {
    TEST("bs_epoch_next 连续递增 10 次");
    bs_epoch_state_t epoch;
    bs_epoch_init(&epoch);
    for (uint64_t i = 1; i <= 10; i++) {
        uint64_t next = bs_epoch_next(&epoch);
        ASSERT_EQ(next, i);
    }
    ASSERT_EQ(epoch.current_epoch, 10ULL);
    PASS();
}

static void test_validate_accept() {
    TEST("bs_epoch_validate 接受合法 epoch (current+1)");
    bs_epoch_state_t epoch;
    bs_epoch_init(&epoch);
    epoch.current_epoch = 42;
    int rc = bs_epoch_validate(&epoch, 43);
    ASSERT_EQ(rc, 0);
    PASS();
}

static void test_validate_stale() {
    TEST("bs_epoch_validate 拒绝 stale epoch (<= current)");
    bs_epoch_state_t epoch;
    bs_epoch_init(&epoch);
    epoch.current_epoch = 42;

    int rc = bs_epoch_validate(&epoch, 42);  /* 等于 current */
    ASSERT_NE(rc, 0);

    rc = bs_epoch_validate(&epoch, 41);      /* 小于 current */
    ASSERT_NE(rc, 0);

    PASS();
}

static void test_validate_jump() {
    TEST("bs_epoch_validate 拒绝跳跃 epoch (> current+1)");
    bs_epoch_state_t epoch;
    bs_epoch_init(&epoch);
    epoch.current_epoch = 42;

    int rc = bs_epoch_validate(&epoch, 99);  /* 跳跃 */
    ASSERT_NE(rc, 0);

    PASS();
}

static void test_serialize_deserialize() {
    TEST("bs_epoch_serialize / deserialize 往返");
    bs_epoch_state_t epoch;
    bs_epoch_init(&epoch);
    epoch.current_epoch = 100;
    epoch.wal_epoch     = 50;
    epoch.cache_epoch   = 80;

    char* buf = nullptr;
    size_t out_len = 0;
    int rc = bs_epoch_serialize(&epoch, &buf, &out_len);
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(out_len > 0);
    ASSERT_NE(buf, nullptr);

    bs_epoch_state_t restored;
    bs_epoch_init(&restored);
    rc = bs_epoch_deserialize(buf, &restored);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(restored.current_epoch, 100ULL);
    ASSERT_EQ(restored.wal_epoch, 50ULL);
    ASSERT_EQ(restored.cache_epoch, 80ULL);

    free(buf);
    PASS();
}

static void test_persist_load() {
    TEST("bs_epoch_persist / load 往返 (临时文件)");
    std::string path = temp_file_path();

    bs_epoch_state_t epoch;
    bs_epoch_init(&epoch);
    epoch.current_epoch = 200;
    epoch.wal_epoch     = 150;
    epoch.cache_epoch   = 180;

    int rc = bs_epoch_persist(path.c_str(), &epoch);
    ASSERT_EQ(rc, 0);

    bs_epoch_state_t loaded;
    bs_epoch_init(&loaded);
    rc = bs_epoch_load(path.c_str(), &loaded);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(loaded.current_epoch, 200ULL);
    ASSERT_EQ(loaded.wal_epoch, 150ULL);
    ASSERT_EQ(loaded.cache_epoch, 180ULL);

    /* cleanup */
    remove(path.c_str());
    PASS();
}

static void test_persist_file_not_found() {
    TEST("bs_epoch_load 文件不存在返回错误");
    std::string path = temp_file_path();
    remove(path.c_str());  /* 确保文件不存在 */

    bs_epoch_state_t loaded;
    bs_epoch_init(&loaded);
    int rc = bs_epoch_load(path.c_str(), &loaded);
    ASSERT_NE(rc, 0);  /* 期望非零（错误） */
    PASS();
}

/* ─── Main ───────────────────────────────────────────────────────────── */

int main() {
    fprintf(stderr, "\n=== Epoch Version Manager Unit Tests ===\n\n");

    test_init();
    test_next_once();
    test_next_monotonic();
    test_validate_accept();
    test_validate_stale();
    test_validate_jump();
    test_serialize_deserialize();
    test_persist_load();
    test_persist_file_not_found();

    fprintf(stderr, "\n  Results: %d / %d passed\n\n",
            pass_count, test_count);

    return (pass_count == test_count) ? 0 : 1;
}
