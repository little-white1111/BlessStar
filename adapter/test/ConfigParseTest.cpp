#include "bs/kernel/common/bs_status.h"

#include "bs/adapter/parser/config_parse.h"
#include "bs/adapter/parser/config_parse_status.h"
#include "bs/adapter/requirement_filter.h"

#include <cassert>
#include <cstring>

static const char kGolden[] = R"({
  "kernel_version": "0.4.0",
  "adapter_version": "0.4.0",
  "manual_requirements": [],
  "instructions": [
    {
      "type": "test",
      "name": "reload-smoke-1",
      "metadata": {
        "subject_code": "1001.01",
        "tax_rate": "13"
      }
    }
  ]
})";

int main()
{
    bs_adapter_parser_status_set_domain_id(60);

    BsConfigParseResult result = {};
    BsStatus st = bs_adapter_parser_parse_bytes(reinterpret_cast<const uint8_t*>(kGolden),
                                                strlen(kGolden), &result);
    assert(bs_status_is_ok(st));
    assert(result.instructions != nullptr);
    assert(result.active_requirements != nullptr);
    assert(bs_ir_instruction_list_size(result.instructions) == 1u);
    assert(bs_adapter_requirement_filter_verify_instructions(result.instructions,
                                                             result.active_requirements) == 0);
    bs_adapter_parser_result_destroy(&result);

    const char kSyntaxError[] = "{";
    st = bs_adapter_parser_parse_bytes(reinterpret_cast<const uint8_t*>(kSyntaxError),
                                       strlen(kSyntaxError), &result);
    assert(!bs_status_is_ok(st));

    const char kBadType[] = R"({
  "kernel_version": "0.4.0",
  "adapter_version": "0.4.0",
  "instructions": [{"type":"not_in_manifest","name":"x"}]
})";
    st = bs_adapter_parser_parse_bytes(reinterpret_cast<const uint8_t*>(kBadType), strlen(kBadType),
                                       &result);
    assert(bs_status_is_ok(st));
    assert(bs_adapter_requirement_filter_verify_instructions(result.instructions,
                                                             result.active_requirements) != 0);
    bs_adapter_parser_result_destroy(&result);
    return 0;
}
