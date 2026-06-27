/* DAG version: AST compiler tests */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bs/kernel/gate_chain/gate_ast.h>
#include <bs/kernel/gate_chain/gate_chain_types.h>
#include <bs/kernel/gate_chain/gate_chain_serialize.h>
#include <bs/kernel/gate_chain/gate_evaluator.h>

static int test_compile_condition(void)
{
    printf("  test_compile_condition ... ");

    const char* ast = "{\"type\":\"condition\",\"field\":\"amount\",\"op\":\"lt\",\"value\":\"50000\"}";
    bs_gate_chain_t* chain = bs_gate_ast_compile(ast);
    if (!chain) { printf("FAIL: compile returned NULL\n"); return 1; }
    if (!chain->root) { printf("FAIL: root is NULL\n"); bs_gate_chain_free(chain); return 1; }

    size_t count = bs_gate_chain_node_count(chain);
    if (count != 1) { printf("FAIL: expected 1 node, got %zu\n", count); bs_gate_chain_free(chain); return 1; }

    bs_gate_eval_context_t ctx = { .field_key = "amount", .field_value = "1000" };
    bs_gate_eval_result_t result;
    memset(&result, 0, sizeof(result));
    bs_gate_evaluator_evaluate(chain, &ctx, &result);
    if (!result.passed) { printf("FAIL: evaluation should pass\n"); bs_gate_chain_free(chain); return 1; }

    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

static int test_compile_and(void)
{
    printf("  test_compile_and ... ");

    const char* ast = "{\"type\":\"and\","
        "\"left\":{\"type\":\"condition\",\"field\":\"amount\",\"op\":\"lt\",\"value\":\"50000\"},"
        "\"right\":{\"type\":\"condition\",\"field\":\"dept\",\"op\":\"eq\",\"value\":\"finance\"}"
        "}";
    bs_gate_chain_t* chain = bs_gate_ast_compile(ast);
    if (!chain) { printf("FAIL: compile returned NULL\n"); return 1; }
    if (!chain->root) { printf("FAIL: root is NULL\n"); bs_gate_chain_free(chain); return 1; }

    size_t count = bs_gate_chain_node_count(chain);
    if (count != 3) { printf("FAIL: expected 3 nodes, got %zu\n", count); bs_gate_chain_free(chain); return 1; }

    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

static int test_compile_then(void)
{
    printf("  test_compile_then ... ");

    const char* ast = "{\"type\":\"then\","
        "\"when\":{\"type\":\"condition\",\"field\":\"amount\",\"op\":\"gt\",\"value\":\"10000\"},"
        "\"do\":{\"type\":\"action\",\"name\":\"approve\",\"value\":\"director\"}"
        "}";
    bs_gate_chain_t* chain = bs_gate_ast_compile(ast);
    if (!chain) { printf("FAIL: compile returned NULL\n"); return 1; }
    if (!chain->root) { printf("FAIL: root is NULL\n"); bs_gate_chain_free(chain); return 1; }

    size_t count = bs_gate_chain_node_count(chain);
    if (count != 2) { printf("FAIL: expected 2 nodes (condition+action), got %zu\n", count); bs_gate_chain_free(chain); return 1; }

    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

static int test_compile_and_helper(void)
{
    printf("  test_compile_and_helper ... ");

    const char* fk[] = {"amount", "dept"};
    const char* op[] = {"lt", "eq"};
    const char* val[] = {"50000", "finance"};

    bs_gate_chain_t* chain = bs_gate_ast_compile_and(fk, op, val, 2);
    if (!chain) { printf("FAIL: compile_and returned NULL\n"); return 1; }
    if (!chain->root) { printf("FAIL: root is NULL\n"); bs_gate_chain_free(chain); return 1; }

    size_t count = bs_gate_chain_node_count(chain);
    if (count != 3) { printf("FAIL: expected 3 nodes, got %zu\n", count); bs_gate_chain_free(chain); return 1; }

    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

static int test_compile_single_condition_helper(void)
{
    printf("  test_compile_single_condition_helper ... ");

    bs_gate_chain_t* chain = bs_gate_ast_compile_condition("amount", "lt", "50000");
    if (!chain) { printf("FAIL: compile_condition returned NULL\n"); return 1; }
    if (!chain->root) { printf("FAIL: root is NULL\n"); bs_gate_chain_free(chain); return 1; }

    bs_gate_eval_context_t ctx = { .field_key = "amount", .field_value = "1000" };
    bs_gate_eval_result_t result;
    memset(&result, 0, sizeof(result));
    bs_gate_evaluator_evaluate(chain, &ctx, &result);
    if (!result.passed) { printf("FAIL: evaluation should pass\n"); bs_gate_chain_free(chain); return 1; }

    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

int main(void)
{
    printf("gate_ast_compiler_test:\n");
    int fail = 0;
    fail += test_compile_condition();
    fail += test_compile_and();
    fail += test_compile_then();
    fail += test_compile_and_helper();
    fail += test_compile_single_condition_helper();
    printf("  %s\n", fail ? "FAILED" : "ALL PASS");
    return fail ? 1 : 0;
}
