/* IntrospectorLangTest.c — 多语言内省插件单元测试
 *
 * 测试 lang_plugin 注册表 + 各语言插件的扫描函数。
 * MVP 以 lang_c.c 和 lang_plugin 注册表为主。
 */

#include "bs/biz_introspector/lang_plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ── Test helpers ─────────────────────────────────────────────────── */
static int g_test_passed = 0;
static int g_test_failed = 0;

#define TEST(name) do { \
    printf("  TEST: %s ... ", #name); \
    test_##name(); \
    printf("PASSED\n"); \
    g_test_passed++; \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        fprintf(stderr, "FAIL at %s:%d: expected %lld, got %lld\n", \
                __FILE__, __LINE__, (long long)(b), (long long)(a)); \
        g_test_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_NOT_NULL(p) do { \
    if ((p) == NULL) { \
        fprintf(stderr, "FAIL at %s:%d: expected non-NULL\n", \
                __FILE__, __LINE__); \
        g_test_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        fprintf(stderr, "FAIL at %s:%d: expected \"%s\", got \"%s\"\n", \
                __FILE__, __LINE__, (b), (a)); \
        g_test_failed++; \
        return; \
    } \
} while(0)

/* ── 辅助测试注册 ──────────────────────────────────────────────────── */

/* lang_c.c 需要从 extern 引用 */
extern int lang_c_scan(const char* src_dir, char** out_json, size_t* out_len);

/* 测试用 stub */
static int mock_scan(const char* src_dir, char** out_json, size_t* out_len)
{
    (void)src_dir;
    *out_json = strdup("{\"language\":\"mock\",\"fields\":[]}");
    *out_len = strlen(*out_json);
    return 0;
}

/* ── Test: 注册表 ──────────────────────────────────────────────────── */
static void test_register_and_find(void)
{
    lang_plugin_t mock = {
        .language = "mock",
        .extensions = ".mock",
        .scan = mock_scan,
    };

    int ret = lang_plugin_register(&mock);
    ASSERT_EQ(ret, 0);

    const lang_plugin_t* found = lang_plugin_find("mock");
    ASSERT_NOT_NULL(found);
    ASSERT_STR_EQ(found->language, "mock");
    ASSERT_STR_EQ(found->extensions, ".mock");
}

static void test_register_duplicate_overwrites(void)
{
    lang_plugin_t first = { "dupl", ".x", mock_scan };
    lang_plugin_t second = { "dupl", ".y", mock_scan };

    ASSERT_EQ(lang_plugin_register(&first), 0);
    ASSERT_EQ(lang_plugin_register(&second), 0);

    const lang_plugin_t* found = lang_plugin_find("dupl");
    ASSERT_NOT_NULL(found);
    ASSERT_STR_EQ(found->extensions, ".y"); /* second overwrites */
}

static void test_find_nonexistent_returns_null(void)
{
    const lang_plugin_t* found = lang_plugin_find("nonexistent_language_v2");
    ASSERT_EQ(found == NULL, true);
}

/* ── Test: lang_c.c 扫描 ───────────────────────────────────────────── */
static void test_lang_c_scan_basic(void)
{
    /* MVP: 仅验证函数符号可链接 */
    (void)lang_c_scan;
}

/* ── Main ─────────────────────────────────────────────────────────── */
int main(void)
{
    printf("=== IntrospectorLangTest: 多语言内省插件 ===\n\n");

    /* 先重置注册表（测试用） */
    g_lang_plugin_count = 0;

    /* 注册 lang_c 插件 */
    lang_plugin_t c_plugin = {
        .language = "c",
        .extensions = ".c,.h",
        .scan = lang_c_scan,
    };
    lang_plugin_register(&c_plugin);

    TEST(register_and_find);
    TEST(register_duplicate_overwrites);
    TEST(find_nonexistent_returns_null);
    TEST(lang_c_scan_basic);

    printf("\n=== Results: %d passed, %d failed ===\n",
           g_test_passed, g_test_failed);

    return g_test_failed > 0 ? 1 : 0;
}
