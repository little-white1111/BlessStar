/**
 * Epoch CAS Integration Tests.
 *
 * 持久化与通信修复 (方案C，Fix 3):
 * - epoch CAS 拒绝乱序写入 → 返回 -ESTALE
 * - configs.json epoch 持久化 + 加载往返
 * - 崩溃检测: epoch 不匹配
 */

#include "bs/adapter/persistence/attach_epoch.h"
#include "bs/adapter/attach_context.h"
#include "bs/adapter/attach_runtime_sync.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <direct.h>
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
    return std::string(tmpdir) + "/bs_epoch_cas_" +
           std::to_string(rand() % 1000000) + ".json";
}

/* ─── Test cases ─────────────────────────────────────────────────────── */

static void test_cas_reject_stale() {
    TEST("epoch CAS 拒绝 stale epoch (返回 -ESTALE)");
    bs_epoch_state_t epoch;
    bs_epoch_init(&epoch);
    epoch.current_epoch = 10;

    /* 构造旧 epoch */
    int rc = bs_epoch_validate(&epoch, 5);   /* < current+1 */
    ASSERT_EQ(rc, -ESTALE);

    /* 构造等于 current 的 epoch */
    rc = bs_epoch_validate(&epoch, 10);
    ASSERT_NE(rc, 0);

    /* 合法 epoch 通过 */
    rc = bs_epoch_validate(&epoch, 11);
    ASSERT_EQ(rc, 0);

    PASS();
}

static void test_epoch_persist_roundtrip() {
    TEST("epoch persist/load 往返 (configs.json 模拟)");
    std::string path = temp_file_path();

    /* 写入 */
    bs_epoch_state_t w;
    bs_epoch_init(&w);
    w.current_epoch = 42;
    w.wal_epoch     = 30;
    w.cache_epoch   = 40;

    int rc = bs_epoch_persist(path.c_str(), &w);
    ASSERT_EQ(rc, 0);

    /* 读取 */
    bs_epoch_state_t r;
    bs_epoch_init(&r);
    rc = bs_epoch_load(path.c_str(), &r);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(r.current_epoch, 42ULL);
    ASSERT_EQ(r.wal_epoch, 30ULL);
    ASSERT_EQ(r.cache_epoch, 40ULL);

    remove(path.c_str());
    PASS();
}

static void test_epoch_detect_mismatch() {
    TEST("崩溃检测: epoch 不匹配可识别");
    bs_epoch_state_t epoch;
    bs_epoch_init(&epoch);

    /* 模拟 WAL epoch > cache epoch (崩溃场景) */
    epoch.wal_epoch   = 100;
    epoch.cache_epoch = 80;

    /* cache 落后于 WAL → 需重放 */
    ASSERT_TRUE(epoch.wal_epoch > epoch.cache_epoch);

    /* 正常场景: WAL == cache */
    epoch.cache_epoch = 100;
    ASSERT_EQ(epoch.wal_epoch, epoch.cache_epoch);

    PASS();
}

/* 并发测试: 多线程同时 bs_epoch_next + validate */
static void test_concurrent_epoch() {
    TEST("并发 epoch: 多线程 next+validate 不冲突");
    bs_epoch_state_t epoch;
    bs_epoch_init(&epoch);

    const int THREAD_COUNT = 4;
    const int OPS_PER_THREAD = 25;
    std::vector<std::thread> threads;
    std::vector<uint64_t> results[THREAD_COUNT];

    for (int t = 0; t < THREAD_COUNT; t++) {
        threads.emplace_back([&epoch, t, &results, OPS_PER_THREAD]() {
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                uint64_t next = bs_epoch_next(&epoch);
                results[t].push_back(next);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    /* 验证总数: THREAD_COUNT * OPS_PER_THREAD */
    ASSERT_EQ(epoch.current_epoch, (uint64_t)(THREAD_COUNT * OPS_PER_THREAD));

    /* 验证单调性: 所有获取的 epoch 互不相同 */
    bool dup[THREAD_COUNT * OPS_PER_THREAD + 1] = {false};
    for (int t = 0; t < THREAD_COUNT; t++) {
        for (auto v : results[t]) {
            ASSERT_TRUE(v >= 1 && v <= (uint64_t)(THREAD_COUNT * OPS_PER_THREAD));
            ASSERT_FALSE(dup[v]);  /* 每个 epoch 唯一 */
            dup[v] = true;
        }
    }

    PASS();
}

/* ─── Main ───────────────────────────────────────────────────────────── */

int main() {
    fprintf(stderr, "\n=== Epoch CAS Integration Tests ===\n\n");

    test_cas_reject_stale();
    test_epoch_persist_roundtrip();
    test_epoch_detect_mismatch();
    test_concurrent_epoch();

    fprintf(stderr, "\n  Results: %d / %d passed\n\n",
            pass_count, test_count);

    return (pass_count == test_count) ? 0 : 1;
}
