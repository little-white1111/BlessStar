/* DAG version: Gate evaluator DFS tests */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bs/kernel/gate_chain/gate_chain_types.h>
#include <bs/kernel/gate_chain/gate_chain_serialize.h>
#include <bs/kernel/gate_chain/gate_evaluator.h>

static int test_evaluator_empty_chain(void)
{
    printf("  test_evaluator_empty_chain ... ");
    bs_gate_chain_t* chain = bs_gate_chain_create();

    bs_gate_eval_context_t ctx = { .field_key = "amount", .field_value = "1000" };
    bs_gate_eval_result_t result;
    memset(&result, 0, sizeof(result));

    int r = bs_gate_evaluator_evaluate(chain, &ctx, &result);
    if (r != 0) { printf("FAIL: evaluate returned %d\n", r); bs_gate_chain_free(chain); return 1; }
    if (!result.passed) { printf("FAIL: should pass with empty chain\n"); bs_gate_chain_free(chain); return 1; }

    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

static int test_evaluator_single_condition_pass(void)
{
    printf("  test_evaluator_single_condition_pass ... ");
    bs_gate_chain_t* chain = bs_gate_chain_create();

    bs_gate_node_t* cond = bs_gate_node_create("bs_condition", "c1");
    cond->field_key = strdup("amount");
    cond->op = strdup("lt");
    cond->value = strdup("50000");
    cond->layer = BS_GATE_LAYER_DEFAULT;
    chain->root = cond;

    bs_gate_eval_context_t ctx = { .field_key = "amount", .field_value = "1000" };
    bs_gate_eval_result_t result;
    memset(&result, 0, sizeof(result));

    int r = bs_gate_evaluator_evaluate(chain, &ctx, &result);
    if (r != 0) { printf("FAIL: evaluate returned %d\n", r); bs_gate_chain_free(chain); return 1; }
    if (!result.passed) { printf("FAIL: should pass (1000 < 50000)\n"); bs_gate_chain_free(chain); return 1; }

    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

static int test_evaluator_condition_fail(void)
{
    printf("  test_evaluator_condition_fail ... ");
    bs_gate_chain_t* chain = bs_gate_chain_create();

    bs_gate_node_t* cond = bs_gate_node_create("bs_condition", "c1");
    cond->field_key = strdup("amount");
    cond->op = strdup("lt");
    cond->value = strdup("50000");
    cond->layer = BS_GATE_LAYER_DEFAULT;
    chain->root = cond;

    bs_gate_eval_context_t ctx = { .field_key = "amount", .field_value = "100000" };
    bs_gate_eval_result_t result;
    memset(&result, 0, sizeof(result));

    int r = bs_gate_evaluator_evaluate(chain, &ctx, &result);
    if (r != 0) { printf("FAIL: evaluate returned %d\n", r); bs_gate_chain_free(chain); return 1; }
    if (result.passed) { printf("FAIL: should fail (100000 >= 50000)\n"); bs_gate_chain_free(chain); return 1; }

    bs_gate_eval_result_free(&result);
    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

static int test_evaluator_and_pass(void)
{
    printf("  test_evaluator_and_pass ... ");
    bs_gate_chain_t* chain = bs_gate_chain_create();

    bs_gate_node_t* and_node = bs_gate_node_create("bs_logic_and", "and1");

    bs_gate_node_t* c1 = bs_gate_node_create("bs_condition", "c1");
    c1->field_key = strdup("amount"); c1->op = strdup("lt"); c1->value = strdup("50000"); c1->layer = BS_GATE_LAYER_DEFAULT;
    bs_gate_node_link_child(and_node, c1);

    bs_gate_node_t* c2 = bs_gate_node_create("bs_condition", "c2");
    c2->field_key = strdup("dept"); c2->op = strdup("eq"); c2->value = strdup("finance"); c2->layer = BS_GATE_LAYER_POLICY;
    bs_gate_node_link_child(and_node, c2);

    chain->root = and_node;

    bs_gate_eval_context_t ctx = { .field_key = "amount", .field_value = "1000" };
    bs_gate_eval_result_t result;
    memset(&result, 0, sizeof(result));

    int r = bs_gate_evaluator_evaluate(chain, &ctx, &result);
    if (r != 0) { printf("FAIL: evaluate returned %d\n", r); bs_gate_chain_free(chain); return 1; }
    if (!result.passed) { printf("FAIL: AND should pass (amount ok, dept skipped)\n"); bs_gate_chain_free(chain); return 1; }

    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

static int test_evaluator_or_pass(void)
{
    printf("  test_evaluator_or_pass ... ");
    bs_gate_chain_t* chain = bs_gate_chain_create();

    bs_gate_node_t* or_node = bs_gate_node_create("bs_logic_or", "or1");

    bs_gate_node_t* c1 = bs_gate_node_create("bs_condition", "c1");
    c1->field_key = strdup("role"); c1->op = strdup("eq"); c1->value = strdup("admin"); c1->layer = BS_GATE_LAYER_DEFAULT;
    bs_gate_node_link_child(or_node, c1);

    bs_gate_node_t* c2 = bs_gate_node_create("bs_condition", "c2");
    c2->field_key = strdup("role"); c2->op = strdup("eq"); c2->value = strdup("manager"); c2->layer = BS_GATE_LAYER_DEFAULT;
    bs_gate_node_link_child(or_node, c2);

    chain->root = or_node;

    bs_gate_eval_context_t ctx = { .field_key = "role", .field_value = "admin" };
    bs_gate_eval_result_t result;
    memset(&result, 0, sizeof(result));

    int r = bs_gate_evaluator_evaluate(chain, &ctx, &result);
    if (r != 0) { printf("FAIL: evaluate returned %d\n", r); bs_gate_chain_free(chain); return 1; }
    if (!result.passed) { printf("FAIL: OR should pass (role=admin matches)\n"); bs_gate_chain_free(chain); return 1; }

    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

int main(void)
{
    printf("gate_evaluator_test:\n");
    int fail = 0;
    fail += test_evaluator_empty_chain();
    fail += test_evaluator_single_condition_pass();
    fail += test_evaluator_condition_fail();
    fail += test_evaluator_and_pass();
    fail += test_evaluator_or_pass();
    printf("  %s\n", fail ? "FAILED" : "ALL PASS");
    return fail ? 1 : 0;
}
