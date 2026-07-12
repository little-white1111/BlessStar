/**
 * Schema Derive Unit Tests
 *
 * ADR-全链路接通 — bs_schema_derive() 必须正确从 JSON 推断字段类型。
 */

#include "bs/adapter/parser/schema_import_export/schema_derive.h"

#include <cstdio>
#include <cstring>
#include <string>

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

#define ASSERT_GT(a, b)                                                 \
    do { if (!((a) > (b))) { FAIL(#a " > " #b); return; } } while (0)

/* ─── Tests ────────────────────────────────────────────────────────── */

static void test_derive_string(void)
{
    TEST("derive STRING type");

    const char* input = "{\"name\": \"hello\"}";
    bs_derive_result_t result;

    int rc = bs_schema_derive(
        (const uint8_t*)input, strlen(input), &result);
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(result.count > 0);
    ASSERT_EQ(result.fields[0].type, BS_DTYPE_STRING);

    bs_schema_derive_free(&result);
    PASS();
}

static void test_derive_integer(void)
{
    TEST("derive INTEGER type");

    const char* input = "{\"port\": 8080}";
    bs_derive_result_t result;

    int rc = bs_schema_derive(
        (const uint8_t*)input, strlen(input), &result);
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(result.count > 0);
    ASSERT_EQ(result.fields[0].type, BS_DTYPE_INTEGER);

    bs_schema_derive_free(&result);
    PASS();
}

static void test_derive_number(void)
{
    TEST("derive NUMBER (float) type");

    const char* input = "{\"ratio\": 3.14}";
    bs_derive_result_t result;

    int rc = bs_schema_derive(
        (const uint8_t*)input, strlen(input), &result);
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(result.count > 0);
    ASSERT_EQ(result.fields[0].type, BS_DTYPE_NUMBER);

    bs_schema_derive_free(&result);
    PASS();
}

static void test_derive_boolean(void)
{
    TEST("derive BOOLEAN type");

    const char* input = "{\"enabled\": true}";
    bs_derive_result_t result;

    int rc = bs_schema_derive(
        (const uint8_t*)input, strlen(input), &result);
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(result.count > 0);
    ASSERT_EQ(result.fields[0].type, BS_DTYPE_BOOLEAN);

    bs_schema_derive_free(&result);
    PASS();
}

static void test_derive_object(void)
{
    TEST("derive OBJECT type for nested value");

    const char* input = "{\"server\": {\"port\": 8080}}";
    bs_derive_result_t result;

    int rc = bs_schema_derive(
        (const uint8_t*)input, strlen(input), &result);
    ASSERT_EQ(rc, 0);
    ASSERT_GT(result.count, 1);

    /* First field should be "server" (OBJECT) */
    bool has_server = false;
    for (size_t i = 0; i < result.count; i++) {
        if (std::string(result.fields[i].key) == "server") {
            has_server = true;
            break;
        }
    }
    ASSERT_TRUE(has_server);

    bs_schema_derive_free(&result);
    PASS();
}

static void test_derive_empty_json(void)
{
    TEST("derive from empty JSON object");

    const char* input = "{}";
    bs_derive_result_t result;

    int rc = bs_schema_derive(
        (const uint8_t*)input, strlen(input), &result);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(result.count, 0);

    bs_schema_derive_free(&result);
    PASS();
}

/* ─── Main ────────────────────────────────────────────────────────── */

int main(void)
{
    fprintf(stderr, "\n=== Schema Derive Unit Tests ===\n\n");

    test_derive_string();
    test_derive_integer();
    test_derive_number();
    test_derive_boolean();
    test_derive_object();
    test_derive_empty_json();

    fprintf(stderr, "\n--- Results: %d / %d passed ---\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
