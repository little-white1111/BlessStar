/**
 * Day13 / T2: security audit hardening (AUD-IX · IMPL-13-11).
 */

#include "bs/kernel/common/bs_status.h"

#include "bs/adapter/parser/config_parse.h"
#include "bs/adapter/parser/config_parse_status.h"
#include "bs/adapter/persistence/attach_store.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <string>

#include "support/config_v1_audit_build.h"
#include "support/test_temp_dir.h"
#include "support/config_v1_security_build.h"

static void assert_parse_fail_schema(const char* json)
{
    BsConfigParseResult result = {};
    const BsStatus      st =
        bs_adapter_parser_parse_bytes(reinterpret_cast<const uint8_t*>(json), std::strlen(json), &result);
    assert(!bs_status_is_ok(st));
    assert(bs_status_code(st) == BS_CONFIG_PARSE_ERR_SCHEMA);
    bs_adapter_parser_result_destroy(&result);
}

static void test_instructions_at_limit_ok_over_fail()
{
    const std::string   ok_json   = bs_test_build_instructions_count(BS_JSON_MAX_INSTRUCTIONS);
    BsConfigParseResult ok_result = {};
    const BsStatus ok_st = bs_adapter_parser_parse_bytes(reinterpret_cast<const uint8_t*>(ok_json.data()),
                                                 ok_json.size(), &ok_result);
    assert(bs_status_is_ok(ok_st));
    bs_adapter_parser_result_destroy(&ok_result);

    const std::string bad_json = bs_test_build_instructions_count(BS_JSON_MAX_INSTRUCTIONS + 1);
    assert_parse_fail_schema(bad_json.c_str());
}

static void test_manual_items_at_limit_ok_over_fail()
{
    const std::string   ok_json = bs_test_build_manual_requirements_count(BS_JSON_MAX_MANUAL_ITEMS);
    BsConfigParseResult ok_result = {};
    const BsStatus ok_st = bs_adapter_parser_parse_bytes(reinterpret_cast<const uint8_t*>(ok_json.data()),
                                                 ok_json.size(), &ok_result);
    assert(bs_status_is_ok(ok_st));
    bs_adapter_parser_result_destroy(&ok_result);

    const std::string bad_json =
        bs_test_build_manual_requirements_count(BS_JSON_MAX_MANUAL_ITEMS + 1);
    assert_parse_fail_schema(bad_json.c_str());
}

static void test_truncated_still_fails_cleanly()
{
    const std::string   json   = bs_test_build_truncated_unclosed_string();
    BsConfigParseResult result = {};
    const BsStatus      st =
        bs_adapter_parser_parse_bytes(reinterpret_cast<const uint8_t*>(json.data()), json.size(), &result);
    assert(!bs_status_is_ok(st));
    bs_adapter_parser_result_destroy(&result);
}

static void test_manifest_line_limit()
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_manifest_line_limit"));
    const std::filesystem::path path = tmp_guard.path / "manifest.bs";
    {
        std::ofstream out(path, std::ios::binary);
        assert(out);
        std::string long_line(static_cast<size_t>(BS_ATTACH_MAX_MANIFEST_LINE) + 1, 'x');
        out << long_line << "\n";
    }
    BsAttachStore* store = bs_adapter_attach_persist_store_open(path.string().c_str());
    assert(store == nullptr);
}

int main()
{
    test_instructions_at_limit_ok_over_fail();
    test_manual_items_at_limit_ok_over_fail();
    test_truncated_still_fails_cleanly();
    test_manifest_line_limit();
    return 0;
}
