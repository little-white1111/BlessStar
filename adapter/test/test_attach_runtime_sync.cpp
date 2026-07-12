/**
 * Runtime Sync Integration Tests.
 *
 * 持久化与通信修复 (方案C，Fix 1):
 * runtime_sync 回调在 ConfigReloadSession::Commit() 成功后自动触发，
 * 将更新后的 runtime_values 同步到 attach 层。
 *
 * 测试覆盖:
 *   1) bs_adapter_attach_set_runtime_sync_fn 注册回调
 *   2) bs_adapter_attach_runtime_sync 触发回调（含 epoch）
 *   3) 未注册回调时不崩溃（空函数指针安全）
 *   4) 回调 ctx 正确传入
 */

#include "bs/adapter/attach_runtime_sync.h"
#include "bs/adapter/attach_context.h"
#include "bs/adapter/persistence/attach_epoch.h"
#include "attach_context_internal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

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

#define ASSERT_STREQ(a, b)                                             \
    do { if (strcmp((a), (b)) != 0) { FAIL(#a " == " #b); return; } } while (0)

/* ─── Test globals ───────────────────────────────────────────────────── */

static char   g_last_key[256] = {0};
static char   g_last_data[4096] = {0};
static size_t g_last_len = 0;
static uint64_t g_last_epoch = 0;
static void*  g_last_ctx = nullptr;
static int    g_callback_count = 0;

/* 测试用回调：记录调用参数 */
static int test_sync_fn(void* ctx, const char* key,
                        const uint8_t* data, size_t len,
                        uint64_t epoch) {
    g_callback_count++;
    g_last_ctx = ctx;
    if (key) {
        strncpy(g_last_key, key, sizeof(g_last_key) - 1);
        g_last_key[sizeof(g_last_key) - 1] = '\0';
    } else {
        g_last_key[0] = '\0';
    }
    if (data && len > 0) {
        size_t copy_len = len < sizeof(g_last_data) - 1 ? len : sizeof(g_last_data) - 1;
        memcpy(g_last_data, data, copy_len);
        g_last_data[copy_len] = '\0';
        g_last_len = copy_len;
    } else {
        g_last_data[0] = '\0';
        g_last_len = 0;
    }
    g_last_epoch = epoch;
    return 0;
}

static void reset_test_globals() {
    g_last_key[0] = '\0';
    g_last_data[0] = '\0';
    g_last_len = 0;
    g_last_epoch = 0;
    g_last_ctx = nullptr;
    g_callback_count = 0;
}

/* ─── Test cases ─────────────────────────────────────────────────────── */

static void test_set_and_sync_fn() {
    TEST("bs_adapter_attach_set_runtime_sync_fn 注册并触发");
    reset_test_globals();

    struct AttachContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    int rc = bs_adapter_attach_set_runtime_sync_fn(&ctx, test_sync_fn, (void*)0x1234);
    ASSERT_EQ(rc, 0);

    const char* key = "test.key";
    const uint8_t data[] = "hello_world";
    uint64_t epoch = 1;

    rc = bs_adapter_attach_runtime_sync(&ctx, key, data, sizeof(data), epoch);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(g_callback_count, 1);
    ASSERT_STREQ(g_last_key, "test.key");
    ASSERT_EQ(g_last_len, sizeof(data));
    ASSERT_EQ(memcmp(g_last_data, data, sizeof(data)), 0);
    ASSERT_EQ(g_last_epoch, 1ULL);
    ASSERT_EQ(g_last_ctx, (void*)0x1234);

    PASS();
}

static void test_no_callback_safety() {
    TEST("未注册回调时触发不崩溃");
    struct AttachContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    const char* key = "no_cb.key";
    const uint8_t data[] = "safe";
    uint64_t epoch = 2;

    int rc = bs_adapter_attach_runtime_sync(&ctx, key, data, sizeof(data), epoch);
    ASSERT_EQ(rc, 0);  /* 无回调时静默返回 0 */
    PASS();
}

static void test_null_epoch() {
    TEST("bs_adapter_attach_runtime_sync epoch=0 传递正确");
    reset_test_globals();

    struct AttachContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    bs_adapter_attach_set_runtime_sync_fn(&ctx, test_sync_fn, nullptr);

    int rc = bs_adapter_attach_runtime_sync(&ctx, "e.test", (const uint8_t*)"data", 4, 0);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(g_callback_count, 1);
    ASSERT_EQ(g_last_epoch, 0ULL);

    PASS();
}

static void test_multi_sync() {
    TEST("多次触发 runtime_sync 回调计数正确");
    reset_test_globals();

    struct AttachContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    bs_adapter_attach_set_runtime_sync_fn(&ctx, test_sync_fn, nullptr);

    for (int i = 1; i <= 5; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        uint8_t data[1] = { (uint8_t)i };
        bs_adapter_attach_runtime_sync(&ctx, key, data, sizeof(data), (uint64_t)i);
    }

    ASSERT_EQ(g_callback_count, 5);
    /* 最后一次调用应该是 key_5 */
    ASSERT_STREQ(g_last_key, "key_5");
    ASSERT_EQ(g_last_data[0], 5);
    ASSERT_EQ(g_last_epoch, 5ULL);

    PASS();
}

static void test_sync_with_epoch_context() {
    TEST("runtime_sync epoch 与 epoch_state 联动");
    reset_test_globals();

    struct AttachContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    bs_adapter_attach_set_runtime_sync_fn(&ctx, test_sync_fn, nullptr);

    bs_epoch_state_t* es = bs_adapter_attach_ctx_epoch_state(&ctx);
    ASSERT_NE(es, nullptr);
    bs_epoch_init(es);

    uint64_t expected = bs_epoch_next(es);  /* 1 */
    ASSERT_EQ(expected, 1ULL);

    bs_adapter_attach_runtime_sync(&ctx, "sync.epoch", (const uint8_t*)"v1", 2, expected);

    ASSERT_EQ(g_callback_count, 1);
    ASSERT_EQ(g_last_epoch, expected);
    ASSERT_EQ(es->current_epoch, 1ULL);

    PASS();
}

/* ─── Main ───────────────────────────────────────────────────────────── */

int main() {
    fprintf(stderr, "\n=== Runtime Sync Integration Tests ===\n\n");

    test_set_and_sync_fn();
    test_no_callback_safety();
    test_null_epoch();
    test_multi_sync();
    test_sync_with_epoch_context();

    fprintf(stderr, "\n  Results: %d / %d passed\n\n",
            pass_count, test_count);

    return (pass_count == test_count) ? 0 : 1;
}
