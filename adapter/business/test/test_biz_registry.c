/**
 * Registry Unit Tests.
 *
 * ADR: 业务系统目录设计与配置注册规范化 (方案C)
 * 覆盖:
 *   - bs_biz_registry_register 正例/反例
 *   - bs_biz_registry_lookup 查询
 *   - bs_biz_registry_list 列表/null 释放
 *   - bs_biz_registry_count
 *   - 重复注册拒绝
 *   - bs_biz_registry_register_normalizer
 *   - bs_biz_get_normalizer
 */

#include "bs/adapter/business/registry.h"
#include "bs/adapter/business/manifest.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

#define ASSERT_TRUE(c)                                                 \
    do { if (!(c)) { FAIL(#c); return; } } while (0)

#define ASSERT_FALSE(c)                                                \
    do { if (c) { FAIL("!" #c); return; } } while (0)

#define ASSERT_NOT_NULL(p)                                             \
    do { if (!(p)) { FAIL(#p " is NULL"); return; } } while (0)

#define ASSERT_NULL(p)                                                 \
    do { if ((p)) { FAIL(#p " is not NULL"); return; } } while (0)

static bs_manifest_t make_manifest(const char* biz_id, const char* display) {
    bs_manifest_t m;
    memset(&m, 0, sizeof(m));
    strncpy(m.biz_id, biz_id, sizeof(m.biz_id) - 1);
    strncpy(m.display_name, display, sizeof(m.display_name) - 1);
    strcpy(m.sdk_version, ">=1.0.0");
    return m;
}

/* ─── Tests ───────────────────────────────────────────────────────────── */

static void test_register_basic() {
    TEST("registry register + count + lookup");
    size_t before = bs_biz_registry_count();
    bs_manifest_t m = make_manifest("test-biz", "测试系统");
    int rc = bs_biz_registry_register(&m);
    ASSERT_EQ(0, rc);
    ASSERT_EQ(before + 1, bs_biz_registry_count());

    const bs_manifest_t* found = bs_biz_registry_lookup("test-biz");
    ASSERT_NOT_NULL(found);
    ASSERT_EQ(0, strcmp(found->biz_id, "test-biz"));
    PASS();
}

static void test_register_duplicate() {
    TEST("registry register 重复返回 -1");
    bs_manifest_t m = make_manifest("dup-biz", "重复系统");
    ASSERT_EQ(0, bs_biz_registry_register(&m));
    ASSERT_EQ(-1, bs_biz_registry_register(&m));
    PASS();
}

static void test_register_null() {
    TEST("registry register(NULL) 返回 -2");
    ASSERT_EQ(-2, bs_biz_registry_register(NULL));
    PASS();
}

static void test_lookup_not_found() {
    TEST("registry lookup 不存在的 biz_id 返回 NULL");
    ASSERT_NULL(bs_biz_registry_lookup("not-exists"));
    PASS();
}

static void test_list() {
    TEST("registry list 返回所有 ID");
    char** ids = NULL;
    size_t count = bs_biz_registry_list(&ids);
    ASSERT_TRUE(count > 0);
    ASSERT_NOT_NULL(ids);
    bs_biz_registry_free_list(ids, count);
    PASS();
}

static void test_list_null_param() {
    TEST("registry list(NULL) 返回 count");
    size_t count = bs_biz_registry_list(NULL);
    ASSERT_TRUE(count > 0);
    PASS();
}

/* ─── Normalizer tests ────────────────────────────────────────────────── */

/* 空函数指针 (不实际调用, 仅测试注册机制) */
static int dummy_normalizer(const char* vendor_id,
                             const char* input,
                             const char* extra,
                             char** out,
                             size_t* out_len) {
    (void)vendor_id; (void)input; (void)extra; (void)out; (void)out_len;
    return 0;
}

static void test_register_normalizer() {
    TEST("register normalizer — 已注册的 biz_id");
    bs_manifest_t m = make_manifest("norm-biz", "归一化测试");
    bs_biz_registry_register(&m);

    int rc = bs_biz_registry_register_normalizer("norm-biz", dummy_normalizer);
    ASSERT_EQ(0, rc);

    BsNormalizerFn fn = bs_biz_get_normalizer("norm-biz");
    ASSERT_EQ(fn, dummy_normalizer);
    PASS();
}

static void test_register_normalizer_unregistered() {
    TEST("register normalizer — 未注册的 biz_id 返回 -1");
    int rc = bs_biz_registry_register_normalizer("no-such-biz", dummy_normalizer);
    ASSERT_EQ(-1, rc);
    PASS();
}

static void test_get_normalizer_none() {
    TEST("get normalizer — 未注册时返回 NULL");
    ASSERT_NULL(bs_biz_get_normalizer("no-normalizer-biz"));
    PASS();
}

/* ─── Main ────────────────────────────────────────────────────────────── */

int main() {
    fprintf(stderr, "\n=== Business: Registry Tests ===\n\n");

    test_register_basic();
    test_register_duplicate();
    test_register_null();
    test_lookup_not_found();
    test_list();
    test_list_null_param();
    test_register_normalizer();
    test_register_normalizer_unregistered();
    test_get_normalizer_none();

    fprintf(stderr, "\n  Results: %d / %d passed\n\n",
            pass_count, test_count);

    return (pass_count == test_count) ? 0 : 1;
}
