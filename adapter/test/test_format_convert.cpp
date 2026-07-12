/**
 * Format Converter Unit Tests
 *
 * ADR-全链路接通 不变量 #5 (格式转换双向幂等性):
 *   bs_format_convert() 必须正确执行所有格式之间的转换。
 *
 * 编译目标: bs_test_fulllink_format_convert
 * 编译依赖: bs_adapter_all
 */

#include "bs/adapter/parser/config_format/format_convert.h"
#include "bs/adapter/parser/config_format/format_types.h"

#include <cstdio>
#include <cstring>
#include <string>

/* ─── Test helpers ─────────────────────────────────────────────────── */

static int test_count    = 0;
static int pass_count    = 0;

#define TEST(name)                                                      \
    do {                                                                \
        test_count++;                                                   \
        fprintf(stderr, "  [TEST] %-60s ", name);                       \
    } while (0)

#define PASS()                                                          \
    do {                                                                \
        pass_count++;                                                   \
        fprintf(stderr, "PASS\n");                                      \
    } while (0)

#define FAIL(msg)                                                       \
    do {                                                                \
        fprintf(stderr, "FAIL: %s\n", msg);                             \
    } while (0)

#define ASSERT_EQ(a, b)                                                 \
    do {                                                                \
        if ((a) != (b)) {                                               \
            FAIL(#a " == " #b);                                         \
            return;                                                     \
        }                                                               \
    } while (0)

#define ASSERT_NE(a, b)                                                 \
    do {                                                                \
        if ((a) == (b)) {                                               \
            FAIL(#a " != " #b);                                         \
            return;                                                     \
        }                                                               \
    } while (0)

#define ASSERT_TRUE(cond)                                               \
    do {                                                                \
        if (!(cond)) {                                                  \
            FAIL(#cond);                                                \
            return;                                                     \
        }                                                               \
    } while (0)

#define ASSERT_NONZERO_OUT(out, out_len)                                \
    do {                                                                \
        ASSERT_NE((out), nullptr);                                      \
        ASSERT_GT((out_len), 0);                                        \
    } while (0)

#define ASSERT_GT(a, b)                                                 \
    do {                                                                \
        if ((a) <= (b)) {                                               \
            FAIL(#a " > " #b);                                          \
            return;                                                     \
        }                                                               \
    } while (0)

/* ─── Test: JSON → JSON (pretty-print) ────────────────────────────── */

static void test_json_to_json(void)
{
    TEST("JSON → JSON pretty-print");

    const char* input  = "{\"a\":1,\"b\":2}";
    uint8_t*    out    = nullptr;
    size_t      out_len = 0;

    int rc = bs_format_convert(
        (const uint8_t*)input, strlen(input),
        BS_FORMAT_JSON, &out, &out_len);

    ASSERT_EQ(rc, 0);
    ASSERT_NONZERO_OUT(out, out_len);

    /* Should be pretty-printed (contain newlines) */
    std::string result((const char*)out, out_len - 1);
    ASSERT_TRUE(result.find('\n') != std::string::npos);

    std::free(out);
    PASS();
}

/* ─── Test: JSON → YAML ───────────────────────────────────────────── */

static void test_json_to_yaml(void)
{
    TEST("JSON → YAML");

    const char* input  = "{\"server\":{\"port\":8080,\"host\":\"localhost\"}}";
    uint8_t*    out    = nullptr;
    size_t      out_len = 0;

    int rc = bs_format_convert(
        (const uint8_t*)input, strlen(input),
        BS_FORMAT_YAML, &out, &out_len);

    ASSERT_EQ(rc, 0);
    ASSERT_NONZERO_OUT(out, out_len);

    std::string result((const char*)out, out_len - 1);
    ASSERT_TRUE(result.find("server") != std::string::npos);
    ASSERT_TRUE(result.find("8080") != std::string::npos);

    std::free(out);
    PASS();
}

/* ─── Test: JSON → TOML ───────────────────────────────────────────── */

static void test_json_to_toml(void)
{
    TEST("JSON → TOML");

    const char* input  = "{\"title\":\"test\",\"count\":42}";
    uint8_t*    out    = nullptr;
    size_t      out_len = 0;

    int rc = bs_format_convert(
        (const uint8_t*)input, strlen(input),
        BS_FORMAT_TOML, &out, &out_len);

    ASSERT_EQ(rc, 0);
    ASSERT_NONZERO_OUT(out, out_len);

    std::string result((const char*)out, out_len - 1);
    ASSERT_TRUE(result.find("title") != std::string::npos);

    std::free(out);
    PASS();
}

/* ─── Test: JSON → INI ────────────────────────────────────────────── */

static void test_json_to_ini(void)
{
    TEST("JSON → INI");

    const char* input  = "{\"server\":{\"port\":8080}}";
    uint8_t*    out    = nullptr;
    size_t      out_len = 0;

    int rc = bs_format_convert(
        (const uint8_t*)input, strlen(input),
        BS_FORMAT_INI, &out, &out_len);

    ASSERT_EQ(rc, 0);
    ASSERT_NONZERO_OUT(out, out_len);

    std::string result((const char*)out, out_len - 1);
    ASSERT_TRUE(result.find("[server]") != std::string::npos);

    std::free(out);
    PASS();
}

/* ─── Test: Error handling ────────────────────────────────────────── */

static void test_invalid_input(void)
{
    TEST("non-JSON content passes through gracefully");

    const char* input  = "{invalid json}";
    uint8_t*    out    = nullptr;
    size_t      out_len = 0;

    /* pretty-print is character-level, doesn't validate JSON structure */
    int rc = bs_format_convert(
        (const uint8_t*)input, strlen(input),
        BS_FORMAT_JSON, &out, &out_len);

    /* Should still produce formatted output */
    ASSERT_EQ(rc, 0);
    ASSERT_NONZERO_OUT(out, out_len);

    std::free(out);
    PASS();
}

static void test_null_input(void)
{
    TEST("NULL input returns error");

    uint8_t* out    = nullptr;
    size_t   out_len = 0;

    int rc = bs_format_convert(nullptr, 0, BS_FORMAT_JSON, &out, &out_len);
    ASSERT_NE(rc, 0);

    PASS();
}

/* ─── Main ────────────────────────────────────────────────────────── */

int main(void)
{
    fprintf(stderr, "\n=== Format Converter Unit Tests ===\n\n");

    test_json_to_json();
    test_json_to_yaml();
    test_json_to_toml();
    test_json_to_ini();
    test_invalid_input();
    test_null_input();

    fprintf(stderr, "\n--- Results: %d / %d passed ---\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
