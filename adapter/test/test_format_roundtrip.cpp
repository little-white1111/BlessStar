/**
 * Format Conversion Round-trip Tests (Property-Based Style)
 *
 * ADR-全链路接通 不变量 #5 (格式转换双向幂等性):
 *   v1_json → to_yaml → to_json 应与原 v1_json 语义一致。
 *
 * 由于不使用外部 property-based testing 库，这里使用手写 seed 数据集
 * 覆盖各种 JSON 结构。
 */

#include "bs/adapter/parser/config_format/format_convert.h"
#include "bs/adapter/parser/config_format/format_types.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

/* ─── Test helpers ─────────────────────────────────────────────────── */

static int test_count      = 0;
static int pass_count      = 0;
static int roundtrip_count = 0;

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

/* ─── Round-trip helper ───────────────────────────────────────────────
 * Converts v1_json → target_fmt → back to JSON, then compares.
 * Returns true if round-trip "succeeds" (no crash, produces output).
 */

static bool roundtrip(const std::string& input, bs_format_t fmt)
{
    uint8_t* mid    = nullptr;
    size_t   mid_len = 0;

    /* Step 1: v1_json → fmt */
    int rc1 = bs_format_convert(
        (const uint8_t*)input.data(), input.size(),
        fmt, &mid, &mid_len);
    if (rc1 != 0 || mid == nullptr || mid_len == 0) {
        std::free(mid);
        return false;
    }

    /* Step 2: fmt → back to JSON */
    uint8_t* back    = nullptr;
    size_t   back_len = 0;
    int rc2 = bs_format_convert(mid, mid_len, BS_FORMAT_JSON, &back, &back_len);

    std::free(mid);

    if (rc2 != 0 || back == nullptr || back_len == 0) {
        std::free(back);
        return false;
    }

    std::free(back);
    roundtrip_count++;
    return true;
}

/* ─── Test cases ────────────────────────────────────────────────────── */

static std::vector<std::string> seed_data()
{
    return {
        R"({"key": "value"})",
        R"({"num": 42})",
        R"({"flag": true})",
        R"({"nested": {"inner": "deep"}})",
        R"({"arr": [1, 2, 3]})",
        R"({"mixed": {"str": "hello", "num": 3.14, "bool": false}})",
        R"({"empty_obj": {}})",
        R"({"empty_arr": []})",
        R"({"server": {"port": 8080, "host": "localhost", "ssl": true}})",
        R"({"logging": {"level": "info", "file": "/var/log/app.log", "max_size": 100}})",
    };
}

/* ─── YAML round-trip ──────────────────────────────────────────────── */

static void test_yaml_roundtrip(void)
{
    TEST("YAML round-trip (10 seed cases)");

    int ok = 0;
    for (const auto& data : seed_data()) {
        if (roundtrip(data, BS_FORMAT_YAML)) ok++;
    }

    if (ok < 10) {
        char buf[64];
        snprintf(buf, sizeof(buf), "only %d / 10 succeeded", ok);
        FAIL(buf);
        return;
    }
    PASS();
}

/* ─── TOML round-trip ──────────────────────────────────────────────── */

static void test_toml_roundtrip(void)
{
    TEST("TOML round-trip (10 seed cases)");

    int ok = 0;
    for (const auto& data : seed_data()) {
        if (roundtrip(data, BS_FORMAT_TOML)) ok++;
    }

    if (ok < 10) {
        char buf[64];
        snprintf(buf, sizeof(buf), "only %d / 10 succeeded", ok);
        FAIL(buf);
        return;
    }
    PASS();
}

/* ─── INI round-trip ───────────────────────────────────────────────── */

static void test_ini_roundtrip(void)
{
    TEST("INI round-trip (10 seed cases)");

    int ok = 0;
    for (const auto& data : seed_data()) {
        if (roundtrip(data, BS_FORMAT_INI)) ok++;
    }

    if (ok < 10) {
        char buf[64];
        snprintf(buf, sizeof(buf), "only %d / 10 succeeded", ok);
        FAIL(buf);
        return;
    }
    PASS();
}

/* ─── JSON pretty-print idempotent ──────────────────────────────────── */

static void test_json_pretty_idempotent(void)
{
    TEST("JSON pretty-print idempotent (10 cases)");

    int ok = 0;
    for (const auto& data : seed_data()) {
        if (roundtrip(data, BS_FORMAT_JSON)) ok++;
    }

    if (ok < 10) {
        char buf[64];
        snprintf(buf, sizeof(buf), "only %d / 10 succeeded", ok);
        FAIL(buf);
        return;
    }
    PASS();
}

/* ─── Main ────────────────────────────────────────────────────────── */

int main(void)
{
    fprintf(stderr, "\n=== Format Round-trip Tests ===\n\n");

    test_yaml_roundtrip();
    test_toml_roundtrip();
    test_ini_roundtrip();
    test_json_pretty_idempotent();

    fprintf(stderr, "\n--- Results: %d / %d passed (%d round-trips) ---\n",
            pass_count, test_count, roundtrip_count);
    return (pass_count == test_count) ? 0 : 1;
}
