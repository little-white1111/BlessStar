/* DAG version: Gate factory produce test */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bs/kernel/gate_chain/gate_chain_types.h>
#include <bs/kernel/gate_chain/gate_factory.h>

static int test_default_factory(void)
{
    printf("  test_default_factory ... ");

    const bs_gate_factory_t* f = bs_default_factory();
    if (!f) { printf("FAIL: default factory is NULL\n"); return 1; }
    if (strcmp(f->name, "default") != 0) { printf("FAIL: factory name mismatch\n"); return 1; }

    bs_gate_rule_def_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.field_key = "amount_limit";
    rule.op = "gt";
    rule.value = "50000";
    rule.scenario = "production";

    bs_gate_node_t* node = NULL;
    int r = bs_gate_factory_produce(f, &rule, &node);
    if (r != 0) { printf("FAIL: produce returned %d\n", r); return 1; }
    if (!node) { printf("FAIL: node is NULL\n"); return 1; }
    if (strcmp(node->type, "bs_condition") != 0) { printf("FAIL: type expected bs_condition\n"); bs_gate_factory_free_node(node); return 1; }
    if (strcmp(node->field_key, "amount_limit") != 0) { printf("FAIL: field_key mismatch\n"); bs_gate_factory_free_node(node); return 1; }
    if (strcmp(node->op, "gt") != 0) { printf("FAIL: op mismatch\n"); bs_gate_factory_free_node(node); return 1; }
    if (strcmp(node->value, "50000") != 0) { printf("FAIL: value mismatch\n"); bs_gate_factory_free_node(node); return 1; }
    if (node->layer != BS_GATE_LAYER_DEFAULT) { printf("FAIL: layer mismatch\n"); bs_gate_factory_free_node(node); return 1; }
    if (!node->stable_key) { printf("FAIL: stable_key is NULL\n"); bs_gate_factory_free_node(node); return 1; }
    if (!node->sub_category) { printf("FAIL: sub_category is NULL\n"); bs_gate_factory_free_node(node); return 1; }
    if (strcmp(node->sub_category, "threshold") != 0) { printf("FAIL: sub_category expected 'threshold', got '%s'\n", node->sub_category); bs_gate_factory_free_node(node); return 1; }

    bs_gate_factory_free_node(node);
    printf("OK\n");
    return 0;
}

static int test_policy_factory(void)
{
    printf("  test_policy_factory ... ");

    const bs_gate_factory_t* f = bs_policy_factory();
    if (!f) { printf("FAIL: policy factory is NULL\n"); return 1; }

    bs_gate_rule_def_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.field_key = "approval_level";
    rule.op = "eq";
    rule.value = "director";

    bs_gate_node_t* node = NULL;
    int r = bs_gate_factory_produce(f, &rule, &node);
    if (r != 0) { printf("FAIL: produce policy returned %d\n", r); return 1; }
    if (!node) { printf("FAIL: node is NULL\n"); return 1; }
    if (strcmp(node->type, "bs_policy_attr") != 0) { printf("FAIL: type expected bs_policy_attr\n"); bs_gate_factory_free_node(node); return 1; }
    if (node->layer != BS_GATE_LAYER_POLICY) { printf("FAIL: layer not policy\n"); bs_gate_factory_free_node(node); return 1; }
    if (strcmp(node->sub_category, "format") != 0) {
        printf("FAIL: sub_category expected 'format', got '%s'\n", node->sub_category ? node->sub_category : "(null)");
        bs_gate_factory_free_node(node); return 1;
    }

    bs_gate_factory_free_node(node);
    printf("OK\n");
    return 0;
}

static int test_custom_factory(void)
{
    printf("  test_custom_factory ... ");

    const bs_gate_factory_t* f = bs_custom_factory();
    if (!f) { printf("FAIL: custom factory is NULL\n"); return 1; }

    bs_gate_rule_def_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.field_key = "fraud_flag";
    rule.op = "eq";
    rule.value = "1";
    rule.ai_hint = "custom fraud detection gate";

    bs_gate_node_t* node = NULL;
    int r = bs_gate_factory_produce(f, &rule, &node);
    if (r != 0) { printf("FAIL: produce custom returned %d\n", r); return 1; }
    if (!node) { printf("FAIL: node is NULL\n"); return 1; }
    if (strcmp(node->type, "bs_custom_gate") != 0) { printf("FAIL: type expected bs_custom_gate\n"); bs_gate_factory_free_node(node); return 1; }
    if (node->layer != BS_GATE_LAYER_CUSTOM) { printf("FAIL: layer not custom\n"); bs_gate_factory_free_node(node); return 1; }

    bs_gate_factory_free_node(node);
    printf("OK\n");
    return 0;
}

static int test_factory_by_layer(void)
{
    printf("  test_factory_by_layer ... ");

    const bs_gate_factory_t* fd = bs_gate_factory_by_layer(BS_GATE_LAYER_DEFAULT);
    if (!fd || strcmp(fd->name, "default") != 0) { printf("FAIL: by_layer default\n"); return 1; }

    const bs_gate_factory_t* fp = bs_gate_factory_by_layer(BS_GATE_LAYER_POLICY);
    if (!fp || strcmp(fp->name, "policy") != 0) { printf("FAIL: by_layer policy\n"); return 1; }

    const bs_gate_factory_t* fc = bs_gate_factory_by_layer(BS_GATE_LAYER_CUSTOM);
    if (!fc || strcmp(fc->name, "custom") != 0) { printf("FAIL: by_layer custom\n"); return 1; }

    printf("OK\n");
    return 0;
}

static int test_factory_null_input(void)
{
    printf("  test_factory_null_input ... ");

    int r = bs_gate_factory_produce(NULL, NULL, NULL);
    if (r == 0) { printf("FAIL: null produce should fail\n"); return 1; }

    printf("OK\n");
    return 0;
}

int main(void)
{
    printf("gate_factory_test:\n");
    int fail = 0;
    fail += test_default_factory();
    fail += test_policy_factory();
    fail += test_custom_factory();
    fail += test_factory_by_layer();
    fail += test_factory_null_input();
    printf("  %s\n", fail ? "FAILED" : "ALL PASS");
    return fail ? 1 : 0;
}
