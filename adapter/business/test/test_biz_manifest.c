/**
 * Manifest & Version Check & Scanner Unit Tests.
 *
 * ADR: 业务系统目录设计与配置注册规范化 (方案C)
 * 覆盖:
 *   - bs_manifest_destroy (null/partial/full)
 *   - bs_version_compatible 正例/反例
 *   - bs_biz_scanner_scan 空目录
 */

#include "bs/adapter/business/manifest.h"
#include "bs/adapter/business/version_check.h"
#include "bs/adapter/business/scanner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

/* ─── Manifest destroy tests ──────────────────────────────────────────── */

static void test_manifest_destroy_null() {
    TEST("bs_manifest_destroy(NULL) 不崩溃");
    bs_manifest_destroy(NULL);
    PASS();
}

static void test_manifest_destroy_empty() {
    TEST("bs_manifest_destroy(empty) 不崩溃");
    bs_manifest_t m;
    memset(&m, 0, sizeof(m));
    bs_manifest_destroy(&m);
    PASS();
}

static void test_manifest_destroy_full() {
    TEST("bs_manifest_destroy(full) 释放所有子字段");
    bs_manifest_t m;
    memset(&m, 0, sizeof(m));
    strcpy(m.biz_id, "test-biz");
    strcpy(m.display_name, "测试系统");

    /* 字段 */
    m.fields_count = 1;
    m.fields = (bs_manifest_field_t*)calloc(1, sizeof(bs_manifest_field_t));
    assert(m.fields);
    strcpy(m.fields[0].key, "room_id");

    bs_manifest_destroy(&m);
    PASS();
}

/* ─── Version check tests ─────────────────────────────────────────────── */

static void test_version_compatible_exact() {
    TEST("bs_version_compatible: =1.0.0 匹配 1.0.0");
    ASSERT_EQ(0, bs_version_compatible("1.0.0", "=1.0.0"));
    PASS();
}

static void test_version_compatible_ge() {
    TEST("bs_version_compatible: >=1.0.0 匹配 2.0.0");
    ASSERT_EQ(0, bs_version_compatible("2.0.0", ">=1.0.0"));
    PASS();
}

static void test_version_compatible_range() {
    TEST("bs_version_compatible: >=1.0.0 <2.0.0 匹配 1.5.0");
    ASSERT_EQ(0, bs_version_compatible("1.5.0", ">=1.0.0 <2.0.0"));
    PASS();
}

static void test_version_compatible_out_of_range() {
    TEST("bs_version_compatible: >=1.0.0 <2.0.0 拒绝 2.5.0");
    ASSERT_EQ(-1, bs_version_compatible("2.5.0", ">=1.0.0 <2.0.0"));
    PASS();
}

static void test_version_compatible_caret() {
    TEST("bs_version_compatible: ^1.2.3 匹配 1.5.0");
    ASSERT_EQ(0, bs_version_compatible("1.5.0", "^1.2.3"));
    PASS();
}

static void test_version_compatible_caret_out() {
    TEST("bs_version_compatible: ^1.2.3 拒绝 2.0.0");
    ASSERT_EQ(-1, bs_version_compatible("2.0.0", "^1.2.3"));
    PASS();
}

static void test_version_compatible_tilde() {
    TEST("bs_version_compatible: ~1.2.3 匹配 1.2.9");
    ASSERT_EQ(0, bs_version_compatible("1.2.9", "~1.2.3"));
    PASS();
}

static void test_version_compatible_tilde_out() {
    TEST("bs_version_compatible: ~1.2.3 拒绝 1.3.0");
    ASSERT_EQ(-1, bs_version_compatible("1.3.0", "~1.2.3"));
    PASS();
}

static void test_version_compatible_gt() {
    TEST("bs_version_compatible: >1.0.0 拒绝 1.0.0");
    ASSERT_EQ(-1, bs_version_compatible("1.0.0", ">1.0.0"));
    PASS();
}

static void test_version_compatible_le() {
    TEST("bs_version_compatible: <=2.0.0 匹配 1.9.9");
    ASSERT_EQ(0, bs_version_compatible("1.9.9", "<=2.0.0"));
    PASS();
}

static void test_version_compatible_null() {
    TEST("bs_version_compatible(NULL, NULL) 返回 -2");
    ASSERT_EQ(-2, bs_version_compatible(NULL, NULL));
    PASS();
}

/* ─── Scanner tests ───────────────────────────────────────────────────── */

static void test_scanner_null() {
    TEST("bs_biz_scanner_scan(NULL) 返回错误");
    bs_manifest_t* manifests = NULL;
    size_t count = 0;
    int rc = bs_biz_scanner_scan(NULL, &manifests, &count);
    ASSERT_TRUE(rc != 0);
    PASS();
}

/* ─── Main ────────────────────────────────────────────────────────────── */

int main() {
    fprintf(stderr, "\n=== Business: Manifest & Version & Scanner Tests ===\n\n");

    test_manifest_destroy_null();
    test_manifest_destroy_empty();
    test_manifest_destroy_full();

    test_version_compatible_exact();
    test_version_compatible_ge();
    test_version_compatible_range();
    test_version_compatible_out_of_range();
    test_version_compatible_caret();
    test_version_compatible_caret_out();
    test_version_compatible_tilde();
    test_version_compatible_tilde_out();
    test_version_compatible_gt();
    test_version_compatible_le();
    test_version_compatible_null();

    test_scanner_null();

    fprintf(stderr, "\n  Results: %d / %d passed\n\n",
            pass_count, test_count);

    return (pass_count == test_count) ? 0 : 1;
}
