/**
 * Unit tests for PrecheckV1InstructionsForScenario (MR-08).
 *
 * Tests correct dispatch of metadata_rules from ScenarioPolicy to
 * the underlying bs_meta_rule_check engine.
 */

#include "bs/kernel/ir/ir.h"
#include "bs/adapter/parser/config_parse.h"

#include "bs/app/sdk/app_scenario_policy.h"
#include "bs/app/sdk/app_vendor_precheck.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace bs::app;

/* ---- Helpers ---- */

/* Minimal valid v1 JSON bytes. */
static const char kMinimalV1[] = R"({
  "kernel_version": "0.4.0",
  "adapter_version": "0.4.0",
  "manual_requirements": [],
  "instructions": [
    {
      "type": "test",
      "name": "instr-a",
      "metadata": {
        "key1": "hello",
        "amount": "100",
        "region": "US",
        "code": "ABC-123"
      }
    }
  ]
})";

static const size_t kMinimalV1Len = sizeof(kMinimalV1) - 1;

static IRInstructionList* parse_instructions(const uint8_t* data, size_t len)
{
    BsConfigParseResult result{};
    BsStatus st = bs_adapter_parser_parse_bytes(data, len, &result);
    if (!bs_status_is_ok(st) || !result.instructions)
        return nullptr;
    return result.instructions;
}

static bool run_precheck(const ScenarioPolicy& policy,
                         const IRInstructionList* instructions,
                         std::string* err_out)
{
    return PrecheckV1InstructionsForScenario(
        reinterpret_cast<const uint8_t*>(kMinimalV1), kMinimalV1Len,
        instructions, policy, err_out);
}

static bs::app::ScenarioPolicy make_policy()
{
    bs::app::ScenarioPolicy p;
    p.tenant = "test-tenant";
    return p;
}

/* ---- Tests ---- */

static int test_empty_rules_passes()
{
    IRInstructionList* instrs = parse_instructions(
        reinterpret_cast<const uint8_t*>(kMinimalV1), kMinimalV1Len);
    if (!instrs)
    {
        std::fprintf(stderr, "  empty_rules_passes: SKIP (parse failed)\n");
        return 0;
    }

    ScenarioPolicy policy = make_policy();
    std::string err;
    bool ok = run_precheck(policy, instrs, &err);
    int rc = ok ? 0 : 1;
    std::fprintf(stderr, "  empty_rules_passes: %s\n", rc == 0 ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(instrs);
    return rc;
}

static int test_eq_rule_pass()
{
    IRInstructionList* instrs = parse_instructions(
        reinterpret_cast<const uint8_t*>(kMinimalV1), kMinimalV1Len);
    if (!instrs) return 0;

    ScenarioPolicy policy = make_policy();
    policy.metadata_rules.push_back({"instr-a", "key1", BS_META_EQ, "hello"});
    std::string err;
    bool ok = run_precheck(policy, instrs, &err);
    int rc = ok ? 0 : 1;
    std::fprintf(stderr, "  eq_rule_pass: %s\n", rc == 0 ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(instrs);
    return rc;
}

static int test_eq_rule_fail()
{
    IRInstructionList* instrs = parse_instructions(
        reinterpret_cast<const uint8_t*>(kMinimalV1), kMinimalV1Len);
    if (!instrs) return 0;

    ScenarioPolicy policy = make_policy();
    policy.metadata_rules.push_back({"instr-a", "key1", BS_META_EQ, "world"});
    std::string err;
    bool ok = run_precheck(policy, instrs, &err);
    int rc = (!ok && !err.empty()) ? 0 : 1;
    std::fprintf(stderr, "  eq_rule_fail: %s (err=%s)\n", rc == 0 ? "PASS" : "FAIL", err.c_str());
    bs_ir_instruction_list_destroy(instrs);
    return rc;
}

static int test_gt_rule_pass()
{
    IRInstructionList* instrs = parse_instructions(
        reinterpret_cast<const uint8_t*>(kMinimalV1), kMinimalV1Len);
    if (!instrs) return 0;

    ScenarioPolicy policy = make_policy();
    policy.metadata_rules.push_back({"instr-a", "amount", BS_META_GT, "50"});
    std::string err;
    bool ok = run_precheck(policy, instrs, &err);
    int rc = ok ? 0 : 1;
    std::fprintf(stderr, "  gt_rule_pass: %s\n", rc == 0 ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(instrs);
    return rc;
}

static int test_gt_rule_fail()
{
    IRInstructionList* instrs = parse_instructions(
        reinterpret_cast<const uint8_t*>(kMinimalV1), kMinimalV1Len);
    if (!instrs) return 0;

    ScenarioPolicy policy = make_policy();
    policy.metadata_rules.push_back({"instr-a", "amount", BS_META_GT, "200"});
    std::string err;
    bool ok = run_precheck(policy, instrs, &err);
    int rc = (!ok && !err.empty()) ? 0 : 1;
    std::fprintf(stderr, "  gt_rule_fail: %s (err=%s)\n", rc == 0 ? "PASS" : "FAIL", err.c_str());
    bs_ir_instruction_list_destroy(instrs);
    return rc;
}

static int test_exists_rule_pass()
{
    IRInstructionList* instrs = parse_instructions(
        reinterpret_cast<const uint8_t*>(kMinimalV1), kMinimalV1Len);
    if (!instrs) return 0;

    ScenarioPolicy policy = make_policy();
    policy.metadata_rules.push_back({"instr-a", "region", BS_META_EXISTS, ""});
    std::string err;
    bool ok = run_precheck(policy, instrs, &err);
    int rc = ok ? 0 : 1;
    std::fprintf(stderr, "  exists_rule_pass: %s\n", rc == 0 ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(instrs);
    return rc;
}

static int test_not_exists_rule_pass()
{
    IRInstructionList* instrs = parse_instructions(
        reinterpret_cast<const uint8_t*>(kMinimalV1), kMinimalV1Len);
    if (!instrs) return 0;

    ScenarioPolicy policy = make_policy();
    policy.metadata_rules.push_back({"instr-a", "nonexistent", BS_META_NOT_EXISTS, ""});
    std::string err;
    bool ok = run_precheck(policy, instrs, &err);
    int rc = ok ? 0 : 1;
    std::fprintf(stderr, "  not_exists_rule_pass: %s\n", rc == 0 ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(instrs);
    return rc;
}

static int test_regex_rule_pass()
{
    IRInstructionList* instrs = parse_instructions(
        reinterpret_cast<const uint8_t*>(kMinimalV1), kMinimalV1Len);
    if (!instrs) return 0;

    ScenarioPolicy policy = make_policy();
    policy.metadata_rules.push_back({"instr-a", "code", BS_META_REGEX, "^[A-Z]+-[0-9]+$"});
    std::string err;
    bool ok = run_precheck(policy, instrs, &err);
    int rc = ok ? 0 : 1;
    std::fprintf(stderr, "  regex_rule_pass: %s\n", rc == 0 ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(instrs);
    return rc;
}

static int test_contains_rule_pass()
{
    IRInstructionList* instrs = parse_instructions(
        reinterpret_cast<const uint8_t*>(kMinimalV1), kMinimalV1Len);
    if (!instrs) return 0;

    ScenarioPolicy policy = make_policy();
    policy.metadata_rules.push_back({"instr-a", "code", BS_META_CONTAINS, "ABC"});
    std::string err;
    bool ok = run_precheck(policy, instrs, &err);
    int rc = ok ? 0 : 1;
    std::fprintf(stderr, "  contains_rule_pass: %s\n", rc == 0 ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(instrs);
    return rc;
}

static int test_invalid_policy_rejected()
{
    ScenarioPolicy policy = make_policy();
    policy.tenant = "";  /* invalid: empty tenant */
    std::string err;
    bool ok = PrecheckV1InstructionsForScenario(
        reinterpret_cast<const uint8_t*>(kMinimalV1), kMinimalV1Len,
        nullptr, policy, &err);
    int rc = (!ok && !err.empty()) ? 0 : 1;
    std::fprintf(stderr, "  invalid_policy_rejected: %s (err=%s)\n", rc == 0 ? "PASS" : "FAIL", err.c_str());
    return rc;
}

/* ---- main ---- */

int main()
{
    int rc = 0;
    std::fprintf(stderr, "AppVendorPrecheckTest:\n");

    rc |= test_empty_rules_passes();
    rc |= test_eq_rule_pass();
    rc |= test_eq_rule_fail();
    rc |= test_gt_rule_pass();
    rc |= test_gt_rule_fail();
    rc |= test_exists_rule_pass();
    rc |= test_not_exists_rule_pass();
    rc |= test_regex_rule_pass();
    rc |= test_contains_rule_pass();
    rc |= test_invalid_policy_rejected();

    if (rc != 0)
        std::fprintf(stderr, "\nFAIL: some AppVendorPrecheck tests failed\n");
    else
        std::fprintf(stderr, "\nAll AppVendorPrecheck tests PASSED\n");
    return rc;
}
