/**
 * Runtime Sync Integration Tests — Commit → runtime_sync → values 一致性.
 *
 * 持久化与通信修复 (方案C，Fix 1 + Fix 3):
 * - Commit 后 runtime_sync 触发, callback 收到正确 key/data/epoch
 * - runtime_sync 与 epoch_state 联动: 每次 sync epoch 单调递增
 * - 多次 Commit, callback 收到最新值
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
static uint8_t g_last_data[4096] = {0};
static size_t g_last_len = 0;
static uint64_t g_last_epoch = 0;
static int    g_callback_count = 0;

static int sync_collector(void* ctx, const char* key,
                          const uint8_t* data, size_t len,
                          uint64_t epoch) {
    g_callback_count++;
    if (key) {
        strncpy(g_last_key, key, sizeof(g_last_key) - 1);
        g_last_key[sizeof(g_last_key) - 1] = '\0';
    }
    if (data && len > 0) {
        size_t cplen = len < sizeof(g_last_data) ? len : sizeof(g_last_data) - 1;
        memcpy(g_last_data, data, cplen);
        g_last_data[cplen] = '\0';
        g_last_len = cplen;
    }
    g_last_epoch = epoch;
    return 0;
}

static void reset_globals() {
    g_last_key[0] = '\0';
    g_last_data[0] = '\0';
    g_last_len = 0;
    g_last_epoch = 0;
    g_callback_count = 0;
}

/* ─── Test cases ─────────────────────────────────────────────────────── */

/* 模拟 Commit 后触发 runtime_sync，验证 callback 参数正确 */
static void test_runtime_sync_after_commit() {
    TEST("runtime_sync 模拟 Commit → callback 收到 key/data/epoch");
    reset_globals();

    struct AttachContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    bs_adapter_attach_set_runtime_sync_fn(&ctx, sync_collector, nullptr);

    /* 模拟 epoch 递增 */
    bs_epoch_state_t* es = bs_adapter_attach_ctx_epoch_state(&ctx);
    bs_epoch_init(es);

    /* 模拟提交 key=room_id, data=123 */
    uint64_t ep = bs_epoch_next(es);
    const char* key = "room_id";
    const uint8_t data[] = "123";
    bs_adapter_attach_runtime_sync(&ctx, key, data, sizeof(data), ep);

    ASSERT_EQ(g_callback_count, 1);
    ASSERT_STREQ(g_last_key, "room_id");
    ASSERT_EQ(memcmp(g_last_data, "123", 3), 0);
    ASSERT_EQ(g_last_epoch, 1ULL);

    PASS();
}

/* 模拟多次 Commit，验证 callback 收到最新值 */
static void test_runtime_sync_multiple_commits() {
    TEST("多次 Commit → callback 收到最新值, epoch 递增");
    reset_globals();

    struct AttachContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    bs_adapter_attach_set_runtime_sync_fn(&ctx, sync_collector, nullptr);
    bs_epoch_state_t* es = bs_adapter_attach_ctx_epoch_state(&ctx);
    bs_epoch_init(es);

    /* Commit 1: room_id = "100" */
    uint64_t e1 = bs_epoch_next(es);
    bs_adapter_attach_runtime_sync(&ctx, "room_id", (const uint8_t*)"100", 3, e1);

    /* Commit 2: theme = "dark" */
    uint64_t e2 = bs_epoch_next(es);
    bs_adapter_attach_runtime_sync(&ctx, "theme", (const uint8_t*)"dark", 4, e2);

    ASSERT_EQ(g_callback_count, 2);
    ASSERT_EQ(g_last_epoch, 2ULL);
    ASSERT_STREQ(g_last_key, "theme");
    ASSERT_EQ(memcmp(g_last_data, "dark", 4), 0);

    PASS();
}

/* 验证 runtime_sync 与 epoch 联动: 每次 sync epoch+1 */
static void test_runtime_sync_epoch_monotonic() {
    TEST("runtime_sync epoch 单调递增, 与 epoch_state 一致");
    reset_globals();

    struct AttachContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    bs_adapter_attach_set_runtime_sync_fn(&ctx, sync_collector, nullptr);
    bs_epoch_state_t* es = bs_adapter_attach_ctx_epoch_state(&ctx);
    bs_epoch_init(es);

    uint64_t prev_epoch = 0;
    for (int i = 1; i <= 5; i++) {
        uint64_t ep = bs_epoch_next(es);
        ASSERT_TRUE(ep > prev_epoch);
        char key[32];
        snprintf(key, sizeof(key), "k%d", i);
        bs_adapter_attach_runtime_sync(&ctx, key, (const uint8_t*)"x", 1, ep);
        ASSERT_EQ(g_last_epoch, ep);
        ASSERT_EQ(g_callback_count, i);
        prev_epoch = ep;
    }

    ASSERT_EQ(es->current_epoch, 5ULL);
    PASS();
}

/* 模拟崩溃恢复: epoch_state 通过 persist/load 重建 */
static void test_runtime_sync_crash_recovery() {
    TEST("崩溃恢复: epoch persist/load 后 runtime_sync 延续");
    reset_globals();

    std::string path = std::string(getenv("TEMP") ? getenv("TEMP") : "/tmp") +
                       "/bs_epoch_crash_" + std::to_string(rand() % 1000000) + ".json";

    /* Phase 1: 正常运行, epoch 到 10 */
    {
        struct AttachContext ctx;
        memset(&ctx, 0, sizeof(ctx));
        bs_adapter_attach_set_runtime_sync_fn(&ctx, sync_collector, nullptr);
        bs_epoch_state_t* es = bs_adapter_attach_ctx_epoch_state(&ctx);
        bs_epoch_init(es);

        for (int i = 0; i < 10; i++) bs_epoch_next(es);
        ASSERT_EQ(es->current_epoch, 10ULL);

        /* 持久化 */
        bs_epoch_persist(path.c_str(), es);
    }

    /* Phase 2: 崩溃重启 */
    {
        struct AttachContext ctx;
        memset(&ctx, 0, sizeof(ctx));
        bs_adapter_attach_set_runtime_sync_fn(&ctx, sync_collector, nullptr);
        bs_epoch_state_t* es = bs_adapter_attach_ctx_epoch_state(&ctx);

        /* 从文件恢复 */
        int rc = bs_epoch_load(path.c_str(), es);
        ASSERT_EQ(rc, 0);
        ASSERT_EQ(es->current_epoch, 10ULL);

        /* 继续递增 */
        uint64_t next = bs_epoch_next(es);
        ASSERT_EQ(next, 11ULL);

        /* runtime_sync 使用恢复后的 epoch */
        bs_adapter_attach_runtime_sync(&ctx, "recovered", (const uint8_t*)"ok", 2, next);
        ASSERT_EQ(g_last_epoch, 11ULL);
    }

    remove(path.c_str());
    PASS();
}

/* ─── Main ───────────────────────────────────────────────────────────── */

int main() {
    fprintf(stderr, "\n=== Runtime Sync Integration Tests ===\n\n");

    test_runtime_sync_after_commit();
    test_runtime_sync_multiple_commits();
    test_runtime_sync_epoch_monotonic();
    test_runtime_sync_crash_recovery();

    fprintf(stderr, "\n  Results: %d / %d passed\n\n",
            pass_count, test_count);

    return (pass_count == test_count) ? 0 : 1;
}
