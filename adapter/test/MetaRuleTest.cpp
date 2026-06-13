/**
 * Unit tests for bs_meta_rule_check (MR-07).
 *
 * Covers:
 *   - All 10 ops (EQ/NE/GT/LT/GE/LE/EXISTS/NOT_EXISTS/REGEX/CONTAINS)
 *   - Empty instr_name (match all)
 *   - Numerical boundary (atof precision)
 *   - Non-existent key
 *   - Multiple rules AND semantics
 *   - NULL/empty param safety
 */

#include "bs/adapter/parser/meta_rule.h"
#include "bs/kernel/ir/ir.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

/* ---- Helpers ---- */

static IRInstructionList* make_list()
{
    return bs_ir_instruction_list_create();
}

static void add_instr(IRInstructionList* list, const char* type, const char* name,
                      const char* k1, const char* v1,
                      const char* k2, const char* v2)
{
    IRInstruction* instr = bs_ir_instruction_create(type, name);
    if (k1)
    {
        IRMetadata* m = bs_ir_metadata_create(k1, v1);
        bs_ir_instruction_add_metadata(instr, m);
    }
    if (k2)
    {
        IRMetadata* m = bs_ir_metadata_create(k2, v2);
        bs_ir_instruction_add_metadata(instr, m);
    }
    bs_ir_instruction_list_add(list, instr);
}

/* ---- Individual test runners ---- */

static int test_all_pass_no_rules()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "key1", "val1", NULL, NULL);

    size_t r = bs_meta_rule_check(list, NULL, 0, NULL, 0);
    int ok = (r == (size_t)-1);
    std::fprintf(stderr, "  all_pass_no_rules: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_eq_match()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "key1", "hello", NULL, NULL);

    BsMetaRule rules[] = {
        {"instr-a", "key1", BS_META_EQ, "hello"}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == (size_t)-1);
    std::fprintf(stderr, "  eq_match: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_eq_no_match()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "key1", "world", NULL, NULL);

    char err[128] = {};
    BsMetaRule rules[] = {
        {"instr-a", "key1", BS_META_EQ, "hello"}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, err, sizeof(err));
    int ok = (r == 0 && std::strlen(err) > 0);
    std::fprintf(stderr, "  eq_no_match: %s (err=%s)\n", ok ? "PASS" : "FAIL", err);
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_ne_match()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "key1", "world", NULL, NULL);

    BsMetaRule rules[] = {
        {"instr-a", "key1", BS_META_NE, "hello"}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == (size_t)-1);
    std::fprintf(stderr, "  ne_match: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_ne_no_match()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "key1", "hello", NULL, NULL);

    BsMetaRule rules[] = {
        {"instr-a", "key1", BS_META_NE, "hello"}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == 0);
    std::fprintf(stderr, "  ne_no_match: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_gt_match()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "amount", "100", NULL, NULL);

    BsMetaRule rules[] = {
        {"instr-a", "amount", BS_META_GT, "50"}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == (size_t)-1);
    std::fprintf(stderr, "  gt_match: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_gt_no_match()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "amount", "30", NULL, NULL);

    BsMetaRule rules[] = {
        {"instr-a", "amount", BS_META_GT, "50"}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == 0);
    std::fprintf(stderr, "  gt_no_match: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_ge_eq()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "amount", "50", NULL, NULL);

    BsMetaRule rules[] = {
        {"instr-a", "amount", BS_META_GE, "50"}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == (size_t)-1);
    std::fprintf(stderr, "  ge_eq: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_lt_match()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "amount", "10", NULL, NULL);

    BsMetaRule rules[] = {
        {"instr-a", "amount", BS_META_LT, "50"}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == (size_t)-1);
    std::fprintf(stderr, "  lt_match: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_lt_no_match()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "amount", "50", NULL, NULL);

    BsMetaRule rules[] = {
        {"instr-a", "amount", BS_META_LT, "50"}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == 0);
    std::fprintf(stderr, "  lt_no_match: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_le_eq()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "amount", "50", NULL, NULL);

    BsMetaRule rules[] = {
        {"instr-a", "amount", BS_META_LE, "50"}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == (size_t)-1);
    std::fprintf(stderr, "  le_eq: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_exists_match()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "key1", "exists_val", NULL, NULL);

    BsMetaRule rules[] = {
        {"instr-a", "key1", BS_META_EXISTS, NULL}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == (size_t)-1);
    std::fprintf(stderr, "  exists_match: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_exists_no_match()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "key1", "val1", NULL, NULL);

    BsMetaRule rules[] = {
        {"instr-a", "nonexistent", BS_META_EXISTS, NULL}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == 0);
    std::fprintf(stderr, "  exists_no_match: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_not_exists_match()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "key1", "val1", NULL, NULL);

    BsMetaRule rules[] = {
        {"instr-a", "nonexistent", BS_META_NOT_EXISTS, NULL}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == (size_t)-1);
    std::fprintf(stderr, "  not_exists_match: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_not_exists_no_match()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "key1", "val1", NULL, NULL);

    BsMetaRule rules[] = {
        {"instr-a", "key1", BS_META_NOT_EXISTS, NULL}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == 0);
    std::fprintf(stderr, "  not_exists_no_match: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_regex_match()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "code", "ABC-123", NULL, NULL);

    BsMetaRule rules[] = {
        {"instr-a", "code", BS_META_REGEX, "^[A-Z]+-[0-9]+$"}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == (size_t)-1);
    std::fprintf(stderr, "  regex_match: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_regex_no_match()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "code", "abc-123", NULL, NULL);

    BsMetaRule rules[] = {
        {"instr-a", "code", BS_META_REGEX, "^[A-Z]+-[0-9]+$"}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == 0);
    std::fprintf(stderr, "  regex_no_match: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_contains_match()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "desc", "hello world foo", NULL, NULL);

    BsMetaRule rules[] = {
        {"instr-a", "desc", BS_META_CONTAINS, "world"}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == (size_t)-1);
    std::fprintf(stderr, "  contains_match: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_contains_no_match()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "desc", "hello world", NULL, NULL);

    BsMetaRule rules[] = {
        {"instr-a", "desc", BS_META_CONTAINS, "foo"}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == 0);
    std::fprintf(stderr, "  contains_no_match: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

/* ---- Edge cases ---- */

static int test_empty_instr_name_matches_all()
{
    IRInstructionList* list = make_list();
    add_instr(list, "typeA", "instr-1", "key1", "val1", NULL, NULL);
    add_instr(list, "typeB", "instr-2", "key1", "val1", NULL, NULL);

    BsMetaRule rules[] = {
        {"", "key1", BS_META_EQ, "val1"}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == (size_t)-1);
    std::fprintf(stderr, "  empty_instr_name_matches_all: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_empty_instr_name_only_matching()
{
    IRInstructionList* list = make_list();
    add_instr(list, "typeA", "instr-1", "key1", "val1", NULL, NULL);
    add_instr(list, "typeB", "instr-2", "key1", "WRONG", NULL, NULL);

    /* With empty instr_name = match all, should fail because instr-2 has WRONG */
    BsMetaRule rules[] = {
        {"", "key1", BS_META_EQ, "val1"}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == 0);
    std::fprintf(stderr, "  empty_instr_name_checks_all: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_nonexistent_key()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "key1", "val1", NULL, NULL);

    BsMetaRule rules[] = {
        {"instr-a", "key_does_not_exist", BS_META_EQ, "val1"}
    };
    char err[128] = {};
    size_t r = bs_meta_rule_check(list, rules, 1, err, sizeof(err));
    int ok = (r == 0 && std::strlen(err) > 0);
    std::fprintf(stderr, "  nonexistent_key: %s (err=%s)\n", ok ? "PASS" : "FAIL", err);
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_multiple_rules_and_semantics()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "amount", "100", "region", "US");

    BsMetaRule rules[] = {
        {"instr-a", "amount", BS_META_GT, "50"},
        {"instr-a", "region", BS_META_EQ, "US"}
    };
    size_t r = bs_meta_rule_check(list, rules, 2, NULL, 0);
    int ok = (r == (size_t)-1);
    std::fprintf(stderr, "  multiple_rules_and: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_multiple_rules_fail_second()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "amount", "100", "region", "CN");

    BsMetaRule rules[] = {
        {"instr-a", "amount", BS_META_GT, "50"},
        {"instr-a", "region", BS_META_EQ, "US"}
    };
    size_t r = bs_meta_rule_check(list, rules, 2, NULL, 0);
    int ok = (r == 1);  /* Second rule should fail */
    std::fprintf(stderr, "  multiple_rules_fail_second: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_name_filter()
{
    IRInstructionList* list = make_list();
    add_instr(list, "typeA", "instr-1", "key1", "val1", NULL, NULL);
    add_instr(list, "typeB", "instr-2", "key1", "val2", NULL, NULL);

    /* Only check instr-1 */
    BsMetaRule rules[] = {
        {"instr-1", "key1", BS_META_EQ, "val1"}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == (size_t)-1);
    std::fprintf(stderr, "  name_filter: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_name_filter_no_match_instruction()
{
    IRInstructionList* list = make_list();
    add_instr(list, "typeA", "instr-1", "key1", "val1", NULL, NULL);

    /* instr-2 does not exist in the list, rule should vacuously pass for non-EXISTS */
    BsMetaRule rules[] = {
        {"instr-2", "key1", BS_META_EQ, "val1"}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == (size_t)-1);
    std::fprintf(stderr, "  name_filter_no_match_instruction: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

static int test_atof_boundary_near_zero()
{
    IRInstructionList* list = make_list();
    add_instr(list, "test", "instr-a", "rate", "0.001", NULL, NULL);

    BsMetaRule rules[] = {
        {"instr-a", "rate", BS_META_GT, "0.0001"}
    };
    size_t r = bs_meta_rule_check(list, rules, 1, NULL, 0);
    int ok = (r == (size_t)-1);
    std::fprintf(stderr, "  atof_boundary_near_zero: %s\n", ok ? "PASS" : "FAIL");
    bs_ir_instruction_list_destroy(list);
    return ok ? 0 : 1;
}

/* ---- main ---- */

int main()
{
    int rc = 0;
    std::fprintf(stderr, "MetaRuleTest:\n");

    rc |= test_all_pass_no_rules();
    rc |= test_eq_match();
    rc |= test_eq_no_match();
    rc |= test_ne_match();
    rc |= test_ne_no_match();
    rc |= test_gt_match();
    rc |= test_gt_no_match();
    rc |= test_ge_eq();
    rc |= test_lt_match();
    rc |= test_lt_no_match();
    rc |= test_le_eq();
    rc |= test_exists_match();
    rc |= test_exists_no_match();
    rc |= test_not_exists_match();
    rc |= test_not_exists_no_match();
    rc |= test_regex_match();
    rc |= test_regex_no_match();
    rc |= test_contains_match();
    rc |= test_contains_no_match();

    /* Edge */
    rc |= test_empty_instr_name_matches_all();
    rc |= test_empty_instr_name_only_matching();
    rc |= test_nonexistent_key();
    rc |= test_multiple_rules_and_semantics();
    rc |= test_multiple_rules_fail_second();
    rc |= test_name_filter();
    rc |= test_name_filter_no_match_instruction();
    rc |= test_atof_boundary_near_zero();

    if (rc != 0)
        std::fprintf(stderr, "\nFAIL: some MetaRule tests failed\n");
    else
        std::fprintf(stderr, "\nAll MetaRule tests PASSED\n");
    return rc;
}
