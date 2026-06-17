/* OPT-08: Gate evaluator tests */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bs/kernel/gate_chain/gate_chain_types.h>
#include <bs/kernel/gate_chain/gate_evaluator.h>

static bs_gate_chain_t* make_chain(void)
{
    bs_gate_chain_t* chain = (bs_gate_chain_t*)calloc(1, sizeof(bs_gate_chain_t));
    chain->version = strdup("1.0");
    return chain;
}

static void add_node(bs_gate_chain_t* chain, const char* type, const char* field_key,
                      const char* op, const char* value, int layer)
{
    size_t idx = chain->node_count;
    chain->node_count++;
    chain->nodes = (bs_gate_node_t*)realloc(chain->nodes,
        chain->node_count * sizeof(bs_gate_node_t));
    memset(&chain->nodes[idx], 0, sizeof(bs_gate_node_t));
    chain->nodes[idx].type      = type ? strdup(type) : NULL;
    chain->nodes[idx].field_key = field_key ? strdup(field_key) : NULL;
    chain->nodes[idx].op        = op ? strdup(op) : NULL;
    chain->nodes[idx].value     = value ? strdup(value) : NULL;
    chain->nodes[idx].layer     = layer;
}

static int test_evaluator_passes_without_nodes(void)
{
    printf("  test_evaluator_passes_without_nodes ... ");

    bs_gate_chain_t* chain = make_chain();
    bs_gate_eval_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.field_key = "amount";
    ctx.field_value = "1000";

    bs_gate_eval_result_t result;
    memset(&result, 0, sizeof(result));

    int r = bs_gate_evaluator_evaluate(chain, &ctx, &result);
    if (r != 0) { printf("FAIL: evaluate returned %d\n", r); bs_gate_chain_free(chain); return 1; }
    if (!result.passed) { printf("FAIL: should pass with empty chain\n"); bs_gate_chain_free(chain); return 1; }

    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

static int test_evaluator_lt_pass(void)
{
    printf("  test_evaluator_lt_pass ... ");

    bs_gate_chain_t* chain = make_chain();
    add_node(chain, "bs_condition", "amount", "lt", "50000", BS_GATE_LAYER_DEFAULT);

    bs_gate_eval_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.field_key = "amount";
    ctx.field_value = "1000";

    bs_gate_eval_result_t result;
    memset(&result, 0, sizeof(result));

    int r = bs_gate_evaluator_evaluate(chain, &ctx, &result);
    if (r != 0) { printf("FAIL: evaluate returned %d\n", r); bs_gate_chain_free(chain); return 1; }
    if (!result.passed) { printf("FAIL: should pass (1000 < 50000)\n"); bs_gate_chain_free(chain); return 1; }

    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

static int test_evaluator_lt_fail(void)
{
    printf("  test_evaluator_lt_fail ... ");

    bs_gate_chain_t* chain = make_chain();
    add_node(chain, "bs_condition", "amount", "lt", "50000", BS_GATE_LAYER_DEFAULT);

    bs_gate_eval_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.field_key = "amount";
    ctx.field_value = "100000";

    bs_gate_eval_result_t result;
    memset(&result, 0, sizeof(result));

    int r = bs_gate_evaluator_evaluate(chain, &ctx, &result);
    if (r != 0) { printf("FAIL: evaluate returned %d\n", r); bs_gate_chain_free(chain); return 1; }
    if (result.passed) { printf("FAIL: should fail (100000 >= 50000)\n"); bs_gate_chain_free(chain); return 1; }
    if (result.failed_node_index != 0) { printf("FAIL: failed node index mismatch\n"); bs_gate_chain_free(chain); return 1; }
    if (!result.error_message) { printf("FAIL: error_message should be set\n"); bs_gate_chain_free(chain); return 1; }

    bs_gate_eval_result_free(&result);
    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

static int test_evaluator_eq_pass(void)
{
    printf("  test_evaluator_eq_pass ... ");

    bs_gate_chain_t* chain = make_chain();
    add_node(chain, "bs_condition", "mode", "eq", "production", BS_GATE_LAYER_POLICY);

    bs_gate_eval_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.field_key = "mode";
    ctx.field_value = "production";

    bs_gate_eval_result_t result;
    memset(&result, 0, sizeof(result));

    int r = bs_gate_evaluator_evaluate(chain, &ctx, &result);
    if (r != 0) { printf("FAIL: evaluate returned %d\n", r); bs_gate_chain_free(chain); return 1; }
    if (!result.passed) { printf("FAIL: should pass (eq match)\n"); bs_gate_chain_free(chain); return 1; }

    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

static int test_evaluator_layer_order(void)
{
    printf("  test_evaluator_layer_order ... ");

    bs_gate_chain_t* chain = make_chain();
    add_node(chain, "bs_condition",   "x", "eq", "1",    BS_GATE_LAYER_DEFAULT);
    add_node(chain, "bs_policy_attr", "y", "gt", "100",  BS_GATE_LAYER_POLICY);
    add_node(chain, "bs_custom_gate", "z", "eq", "ok",   BS_GATE_LAYER_CUSTOM);

    /* Test with field_key=z, custom layer */
    bs_gate_eval_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.field_key = "z";
    ctx.field_value = "ok";

    bs_gate_eval_result_t result;
    memset(&result, 0, sizeof(result));

    int r = bs_gate_evaluator_evaluate(chain, &ctx, &result);
    if (r != 0) { printf("FAIL: evaluate returned %d\n", r); bs_gate_chain_free(chain); return 1; }
    if (!result.passed) { printf("FAIL: should pass (field z matches custom layer eq)\n"); bs_gate_chain_free(chain); return 1; }

    /* Test with field_key=y, wrong value */
    bs_gate_eval_context_t ctx2;
    memset(&ctx2, 0, sizeof(ctx2));
    ctx2.field_key = "y";
    ctx2.field_value = "50";

    bs_gate_eval_result_t result2;
    memset(&result2, 0, sizeof(result2));

    r = bs_gate_evaluator_evaluate(chain, &ctx2, &result2);
    if (r != 0) { printf("FAIL: evaluate 2 returned %d\n", r); bs_gate_chain_free(chain); return 1; }
    if (result2.passed) { printf("FAIL: should fail (50 <= 100)\n"); bs_gate_chain_free(chain); return 1; }
    if (result2.failed_layer != BS_GATE_LAYER_POLICY) {
        printf("FAIL: failed_layer expected %d, got %zu\n", BS_GATE_LAYER_POLICY, result2.failed_layer);
        bs_gate_chain_free(chain); return 1;
    }

    bs_gate_eval_result_free(&result2);
    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

int main(void)
{
    printf("gate_evaluator_test:\n");
    int fail = 0;
    fail += test_evaluator_passes_without_nodes();
    fail += test_evaluator_lt_pass();
    fail += test_evaluator_lt_fail();
    fail += test_evaluator_eq_pass();
    fail += test_evaluator_layer_order();
    printf("  %s\n", fail ? "FAILED" : "ALL PASS");
    return fail ? 1 : 0;
}
