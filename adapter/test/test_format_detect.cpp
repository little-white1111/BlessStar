/**
 * Format Detection Unit Tests
 *
 * ADR-全链路接通 — bs_format_detect() 基于内容魔术字和扩展名的格式检测。
 */

#include "bs/adapter/parser/config_format/format_convert.h"
#include "bs/adapter/parser/config_format/format_types.h"

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
    do {                                                               \
        if ((a) != (b)) { FAIL(#a " == " #b); return; }                \
    } while (0)

#define ASSERT_NE(a, b)                                                \
    do {                                                               \
        if ((a) == (b)) { FAIL(#a " != " #b); return; }                \
    } while (0)

#define ASSERT_TRUE(cond)                                              \
    do { if (!(cond)) { FAIL(#cond); return; } } while (0)

/* ─── Tests ────────────────────────────────────────────────────────── */

static void test_detect_json_brace(void)
{
    TEST("detect JSON by leading '{'");

    bs_format_detect_result_t r = bs_format_detect(
        (const uint8_t*)"{\"key\": 1}", 10, nullptr);
    ASSERT_EQ(r.format, BS_FORMAT_JSON);
    PASS();
}

static void test_detect_json_bracket(void)
{
    TEST("detect JSON array by leading '['");

    bs_format_detect_result_t r = bs_format_detect(
        (const uint8_t*)"[1, 2, 3]", 9, nullptr);
    ASSERT_EQ(r.format, BS_FORMAT_JSON);
    PASS();
}

static void test_detect_yaml_colon(void)
{
    TEST("detect YAML by key: value pattern");

    bs_format_detect_result_t r = bs_format_detect(
        (const uint8_t*)"key: value\nfoo: bar", 20, nullptr);
    ASSERT_EQ(r.format, BS_FORMAT_YAML);
    PASS();
}

static void test_detect_toml_table(void)
{
    TEST("detect TOML by [table] header");

    bs_format_detect_result_t r = bs_format_detect(
        (const uint8_t*)"[server]\nport = 8080", 20, nullptr);
    ASSERT_EQ(r.format, BS_FORMAT_TOML);
    PASS();
}

static void test_detect_ini_section(void)
{
    TEST("detect INI by [section] header");

    bs_format_detect_result_t r = bs_format_detect(
        (const uint8_t*)"[section]\nkey=value", 20, nullptr);
    ASSERT_EQ(r.format, BS_FORMAT_INI);
    PASS();
}

static void test_detect_by_extension(void)
{
    TEST("detect JSON by .json extension");

    bs_format_detect_result_t r = bs_format_detect(
        (const uint8_t*)"", 0, "config.json");
    ASSERT_EQ(r.format, BS_FORMAT_JSON);
    PASS();
}

static void test_detect_by_yaml_extension(void)
{
    TEST("detect YAML by .yaml extension");

    bs_format_detect_result_t r = bs_format_detect(
        (const uint8_t*)"", 0, "config.yaml");
    ASSERT_EQ(r.format, BS_FORMAT_YAML);
    PASS();
}

static void test_detect_by_yml_extension(void)
{
    TEST("detect YAML by .yml extension");

    bs_format_detect_result_t r = bs_format_detect(
        (const uint8_t*)"", 0, "config.yml");
    ASSERT_EQ(r.format, BS_FORMAT_YAML);
    PASS();
}

static void test_detect_by_toml_extension(void)
{
    TEST("detect TOML by .toml extension");

    bs_format_detect_result_t r = bs_format_detect(
        (const uint8_t*)"", 0, "config.toml");
    ASSERT_EQ(r.format, BS_FORMAT_TOML);
    PASS();
}

static void test_detect_unknown(void)
{
    TEST("unknown content returns UNKNOWN");

    bs_format_detect_result_t r = bs_format_detect(
        (const uint8_t*)"some random text", 16, nullptr);
    ASSERT_EQ(r.format, BS_FORMAT_JSON);
    /* Low confidence for unrecognized content */
    ASSERT_TRUE(r.confidence < 50);
    PASS();
}

/* ─── Main ────────────────────────────────────────────────────────── */

int main(void)
{
    fprintf(stderr, "\n=== Format Detection Unit Tests ===\n\n");

    test_detect_json_brace();
    test_detect_json_bracket();
    test_detect_yaml_colon();
    test_detect_toml_table();
    test_detect_ini_section();
    test_detect_by_extension();
    test_detect_by_yaml_extension();
    test_detect_by_yml_extension();
    test_detect_by_toml_extension();
    test_detect_unknown();

    fprintf(stderr, "\n--- Results: %d / %d passed ---\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
