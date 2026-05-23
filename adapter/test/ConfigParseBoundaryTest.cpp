/**
 * Day10 / T1: model C boundary + BS_JSON_MAX_* resource limits (BOUND-IX).
 * Does not validate money semantics (see tools/normalize money_normalize.py).
 */

#include "bs/kernel/common/bs_status.h"
#include "bs/kernel/ir/ir.h"

#include "bs/adapter/parser/config_parse.h"
#include "bs/adapter/requirement_filter.h"

#include <cassert>
#include <cstring>

#include <string>

#include "support/config_v1_boundary_build.h"

static const size_t kMaxInputBytes = 1024u * 1024u;
static const size_t kMaxStringLen  = 4096u;

static void test_model_c_parse_and_gate()
{
    std::string json;
    bs_test_build_config_v1_model_c(json, 400, 15);
    assert(json.size() > 80000u);
    assert(json.size() < kMaxInputBytes);

    BsConfigParseResult result = {};
    BsStatus            st =
        bs_config_parse_bytes(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &result);
    assert(bs_status_is_ok(st));
    assert(result.instructions != nullptr);
    assert(ir_instruction_list_size(result.instructions) == 400u);
    assert(bs_adapter_requirement_filter_verify_instructions(result.instructions,
                                                             result.active_requirements) == 0);

    IRInstruction* first = ir_instruction_list_get(result.instructions, 0);
    assert(first != nullptr);
    const char* amount = ir_instruction_get_metadata(first, "amount");
    assert(amount != nullptr && std::strcmp(amount, "100.50") == 0);
    const char* tax = ir_instruction_get_metadata(first, "tax_rate");
    assert(tax != nullptr && std::strcmp(tax, "13") == 0);

    bs_config_parse_result_destroy(&result);
}

static void test_string_length_4095_ok()
{
    std::string         json   = bs_test_build_single_instruction_metadata_value(4095);
    BsConfigParseResult result = {};
    BsStatus            st =
        bs_config_parse_bytes(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &result);
    assert(bs_status_is_ok(st));
    bs_config_parse_result_destroy(&result);
}

static void test_string_length_4097_fail()
{
    std::string         json   = bs_test_build_single_instruction_metadata_value(4097);
    BsConfigParseResult result = {};
    BsStatus            st =
        bs_config_parse_bytes(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &result);
    assert(!bs_status_is_ok(st));
    bs_config_parse_result_destroy(&result);
}

static void test_input_over_1mb_fail()
{
    std::string json;
    bs_test_build_config_v1_model_c(json, 5500, 15);
    assert(json.size() > kMaxInputBytes);

    BsConfigParseResult result = {};
    BsStatus            st =
        bs_config_parse_bytes(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &result);
    assert(!bs_status_is_ok(st));
    bs_config_parse_result_destroy(&result);
}

static void test_input_near_1mb_ok()
{
    size_t      count = 2000;
    std::string json;
    while (count > 50)
    {
        bs_test_build_config_v1_model_c(json, count, 12);
        if (json.size() < kMaxInputBytes - 512)
            break;
        count -= 50;
    }
    assert(json.size() < kMaxInputBytes);

    BsConfigParseResult result = {};
    BsStatus            st =
        bs_config_parse_bytes(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &result);
    assert(bs_status_is_ok(st));
    bs_config_parse_result_destroy(&result);
}

static void test_empty_metadata_value_ok()
{
    const char          kJson[] = R"({
  "kernel_version": "0.4.0",
  "adapter_version": "0.4.0",
  "instructions": [{
    "type": "test",
    "name": "empty-meta",
    "metadata": { "note": "" }
  }]
})";
    BsConfigParseResult result  = {};
    BsStatus            st =
        bs_config_parse_bytes(reinterpret_cast<const uint8_t*>(kJson), strlen(kJson), &result);
    assert(bs_status_is_ok(st));
    IRInstruction* first = ir_instruction_list_get(result.instructions, 0);
    const char*    note  = ir_instruction_get_metadata(first, "note");
    assert(note != nullptr && note[0] == '\0');
    bs_config_parse_result_destroy(&result);
}

static void test_truncated_json_fail()
{
    std::string         json   = bs_test_build_truncated_json();
    BsConfigParseResult result = {};
    BsStatus            st =
        bs_config_parse_bytes(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &result);
    assert(!bs_status_is_ok(st));
    bs_config_parse_result_destroy(&result);
}

static void test_model_a_light_optional()
{
    std::string json;
    bs_test_build_config_v1_model_a_light(json, 400);
    BsConfigParseResult result = {};
    BsStatus            st =
        bs_config_parse_bytes(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &result);
    assert(bs_status_is_ok(st));
    assert(ir_instruction_list_size(result.instructions) == 400u);
    bs_config_parse_result_destroy(&result);
}

int main()
{
    bs_config_parse_status_set_domain_id(60);
    test_model_c_parse_and_gate();
    test_string_length_4095_ok();
    test_string_length_4097_fail();
    test_input_over_1mb_fail();
    test_input_near_1mb_ok();
    test_empty_metadata_value_ok();
    test_truncated_json_fail();
    test_model_a_light_optional();
    return 0;
}
