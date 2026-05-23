/**
 * Day11 / T2: malformed & security-oriented parser tests (SEC-IX).
 * Money semantics stay in tools/normalize money_normalize.py (not asserted here).
 */

#include "bs/kernel/common/bs_status.h"
#include "bs/kernel/ir/ir.h"

#include "bs/adapter/parser/config_parse.h"
#include "bs/adapter/parser/config_parse_status.h"

#include <cassert>
#include <cstring>

#include "support/config_v1_security_build.h"

static void assert_parse_fail_schema_or_parse(const char* json, size_t min_line)
{
    BsConfigParseResult result = {};
    BsStatus            st =
        bs_config_parse_bytes(reinterpret_cast<const uint8_t*>(json), std::strlen(json), &result);
    assert(!bs_status_is_ok(st));
    assert(result.error_line >= min_line);
    assert(result.error_column > 0);
    bs_config_parse_result_destroy(&result);
}

static void assert_parse_fail_schema(const char* json)
{
    BsConfigParseResult result = {};
    BsStatus            st =
        bs_config_parse_bytes(reinterpret_cast<const uint8_t*>(json), std::strlen(json), &result);
    assert(!bs_status_is_ok(st));
    assert(bs_status_code(st) == BS_CONFIG_PARSE_ERR_SCHEMA);
    assert(result.error_line > 0);
    assert(result.error_column > 0);
    bs_config_parse_result_destroy(&result);
}

static void test_utf8_surrogate_rejected()
{
    const std::string json = bs_test_build_invalid_utf8_surrogate_escape();
    assert_parse_fail_schema_or_parse(json.c_str(), 1);
}

static void test_depth_at_limit_ok_and_over_fail()
{
    const std::string   ok_json   = bs_test_build_security_minimal_ok();
    BsConfigParseResult ok_result = {};
    BsStatus ok_st = bs_config_parse_bytes(reinterpret_cast<const uint8_t*>(ok_json.data()),
                                           ok_json.size(), &ok_result);
    assert(bs_status_is_ok(ok_st));
    bs_config_parse_result_destroy(&ok_result);

    /* Valid fields parse first; unknown "bomb" skip must hit BS_JSON_MAX_DEPTH (32). */
    const std::string bad_json = bs_test_build_config_with_depth_bomb(40);
    assert_parse_fail_schema(bad_json.c_str());
}

static void test_duplicate_metadata_key()
{
    const std::string json = bs_test_build_duplicate_metadata_amount();
    assert_parse_fail_schema(json.c_str());
}

static void test_type_confusion_number_in_metadata()
{
    const std::string json = bs_test_build_type_confusion_tax_rate_number();
    assert_parse_fail_schema(json.c_str());
}

static void test_injection_string_stores_literal()
{
    const std::string   json   = bs_test_build_injection_string_metadata();
    BsConfigParseResult result = {};
    BsStatus            st =
        bs_config_parse_bytes(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &result);
    assert(bs_status_is_ok(st));
    IRInstruction* first = ir_instruction_list_get(result.instructions, 0);
    const char*    sql   = ir_instruction_get_metadata(first, "sql");
    assert(sql != nullptr && std::strstr(sql, "DROP TABLE") != nullptr);
    bs_config_parse_result_destroy(&result);
}

static void test_truncated_json_fail_with_line()
{
    const std::string json = bs_test_build_truncated_unclosed_string();
    assert_parse_fail_schema_or_parse(json.c_str(), 1);
}

static void test_minimal_ok_baseline()
{
    const std::string   json   = bs_test_build_security_minimal_ok();
    BsConfigParseResult result = {};
    BsStatus            st =
        bs_config_parse_bytes(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &result);
    assert(bs_status_is_ok(st));
    bs_config_parse_result_destroy(&result);
}

int main()
{
    bs_config_parse_status_set_domain_id(60);
    test_minimal_ok_baseline();
    test_utf8_surrogate_rejected();
    test_depth_at_limit_ok_and_over_fail();
    test_duplicate_metadata_key();
    test_type_confusion_number_in_metadata();
    test_injection_string_stores_literal();
    test_truncated_json_fail_with_line();
    return 0;
}
