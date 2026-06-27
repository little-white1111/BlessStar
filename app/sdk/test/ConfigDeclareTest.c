/* ConfigDeclareTest: 方案 H 全局配置注册 C ABI 单元测试 */
#include "bs/app/sdk/config_declare.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

#define ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        fprintf(stderr, "FAIL at %s:%d: expected \"%s\", got \"%s\"\n", \
                __FILE__, __LINE__, (b), (a)); \
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

#define ASSERT_NULL(p) do { \
    if ((p) != NULL) { \
        fprintf(stderr, "FAIL at %s:%d: expected NULL\n", \
                __FILE__, __LINE__); \
        g_test_failed++; \
        return; \
    } \
} while(0)

/* ══════════════════════════════════════════════════════════════════
 * Test cases
 * ══════════════════════════════════════════════════════════════════ */

/* 1. Basic declaration */
static void test_basic_declaration(void)
{
    bs_config_declare_reset();

    const bs_field_decl_t fields[] = {
        BS_FIELD("db.host", BS_TYPE_STRING, "localhost", "数据库地址"),
        BS_FIELD("db.port", BS_TYPE_INT32,  "3306",      "端口号"),
        BS_FIELD("db.ssl",  BS_TYPE_BOOL,   "false",     "启用 SSL"),
    };

    int ret = bs_config_declare(fields, 3);
    ASSERT_EQ(ret, 0);

    char* json = NULL;
    size_t len = 0;
    ret = bs_config_declare_get_schema_json(&json, &len);
    ASSERT_EQ(ret, 0);
    ASSERT_NOT_NULL(json);
    ASSERT_EQ(len > 0, true);

    /* Verify JSON content */
    ASSERT_NOT_NULL(strstr(json, "db.host"));
    ASSERT_NOT_NULL(strstr(json, "string"));
    ASSERT_NOT_NULL(strstr(json, "db.port"));
    ASSERT_NOT_NULL(strstr(json, "int32"));
    ASSERT_NOT_NULL(strstr(json, "db.ssl"));
    ASSERT_NOT_NULL(strstr(json, "bool"));
    ASSERT_NOT_NULL(strstr(json, "localhost"));
    ASSERT_NOT_NULL(strstr(json, "数据库地址"));

    free(json);
}

/* 2. Empty field list */
static void test_empty_declaration(void)
{
    bs_config_declare_reset();
    int ret = bs_config_declare(NULL, 0);
    ASSERT_EQ(ret, -1);

    char* json = NULL;
    size_t len = 0;
    ret = bs_config_declare_get_schema_json(&json, &len);
    ASSERT_EQ(ret, -1);  /* no declarations yet */
    ASSERT_NULL(json);
}

/* 3. Duplicate key override */
static void test_duplicate_key_override(void)
{
    bs_config_declare_reset();

    const bs_field_decl_t fields1[] = {
        BS_FIELD("timeout", BS_TYPE_INT32, "30", "超时秒数"),
    };
    bs_config_declare(fields1, 1);

    /* Override with same key but different type and default */
    const bs_field_decl_t fields2[] = {
        BS_FIELD("timeout", BS_TYPE_INT64, "60", "新的超时秒数"),
    };
    bs_config_declare(fields2, 1);

    char* json = NULL;
    size_t len = 0;
    bs_config_declare_get_schema_json(&json, &len);
    ASSERT_NOT_NULL(json);

    fprintf(stderr, "    [debug] duplicate JSON=[%s]\n", json);

    /* Should have the new value */
    ASSERT_NOT_NULL(strstr(json, "int64"));
    ASSERT_NOT_NULL(strstr(json, "60"));
    ASSERT_NULL(strstr(json, "int32"));
    ASSERT_NULL(strstr(json, "30"));

    free(json);
}

/* 4. Required field */
static void test_required_field(void)
{
    bs_config_declare_reset();

    const bs_field_decl_t fields[] = {
        BS_FIELD_REQ("host", BS_TYPE_STRING, "0.0.0.0", "监听地址"),
        BS_FIELD("port", BS_TYPE_INT32, "8080", "端口号"),
    };

    bs_config_declare(fields, 2);

    char* json = NULL;
    size_t len = 0;
    bs_config_declare_get_schema_json(&json, &len);
    ASSERT_NOT_NULL(json);

    ASSERT_NOT_NULL(strstr(json, "\"required\": true"));
    /* port should NOT have required */
    ASSERT_NULL(strstr(json + (strstr(json, "port") - json), "required"));

    free(json);
}

/* 5. Multiple calls non-destructive */
static void test_multiple_calls(void)
{
    bs_config_declare_reset();

    const bs_field_decl_t batch1[] = {
        BS_FIELD("a", BS_TYPE_INT32, "1", "A"),
        BS_FIELD("b", BS_TYPE_INT32, "2", "B"),
    };
    bs_config_declare(batch1, 2);

    const bs_field_decl_t batch2[] = {
        BS_FIELD("c", BS_TYPE_INT32, "3", "C"),
    };
    bs_config_declare(batch2, 1);

    char* json = NULL;
    size_t len = 0;
    bs_config_declare_get_schema_json(&json, &len);
    ASSERT_NOT_NULL(json);

    /* All three fields should be present */
    ASSERT_NOT_NULL(strstr(json, "\"a\""));
    ASSERT_NOT_NULL(strstr(json, "\"b\""));
    ASSERT_NOT_NULL(strstr(json, "\"c\""));

    free(json);
}

/* 6. Reset clears everything */
static void test_reset(void)
{
    bs_config_declare_reset();

    const bs_field_decl_t fields[] = {
        BS_FIELD("x", BS_TYPE_INT32, "0", "X"),
    };
    bs_config_declare(fields, 1);

    bs_config_declare_reset();

    char* json = NULL;
    size_t len = 0;
    int ret = bs_config_declare_get_schema_json(&json, &len);
    ASSERT_EQ(ret, -1);  /* reset cleared it */
    ASSERT_NULL(json);
}

/* 7. JSON structure validation */
static void test_json_structure(void)
{
    bs_config_declare_reset();

    const bs_field_decl_t fields[] = {
        BS_FIELD("pool.max", BS_TYPE_INT32, "100", "最大连接数"),
    };
    bs_config_declare(fields, 1);

    char* json = NULL;
    size_t len = 0;
    bs_config_declare_get_schema_json(&json, &len);
    ASSERT_NOT_NULL(json);

    /* Verify JSON is parseable and has correct structure */
    ASSERT_NOT_NULL(strstr(json, "\"version\": \"1.0\""));
    ASSERT_NOT_NULL(strstr(json, "\"generated_at\""));
    ASSERT_NOT_NULL(strstr(json, "\"fields\""));
    ASSERT_NOT_NULL(strstr(json, "\"key\": \"pool.max\""));
    ASSERT_NOT_NULL(strstr(json, "\"type\": \"int32\""));
    ASSERT_NOT_NULL(strstr(json, "\"default\": \"100\""));
    ASSERT_NOT_NULL(strstr(json, "\"description\": \"最大连接数\""));

    free(json);
}

/* ══════════════════════════════════════════════════════════════════
 * Main
 * ══════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("=== ConfigDeclareTest: 方案 H 全局注册 C ABI ===\n\n");

    TEST(basic_declaration);
    TEST(empty_declaration);
    TEST(duplicate_key_override);
    TEST(required_field);
    TEST(multiple_calls);
    TEST(reset);
    TEST(json_structure);

    printf("\n=== Results: %d passed, %d failed ===\n",
           g_test_passed, g_test_failed);

    return g_test_failed > 0 ? 1 : 0;
}
