/* OPT-08: Gate factory produce + sub_category inference tests */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bs/kernel/gate_chain/gate_factory.h>
#include <bs/kernel/gate_chain/gate_chain_types.h>

static int test_default_factory_threshold(void)
{
    printf("  test_default_factory_threshold ... ");

    bs_gate_rule_def_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.field_key = "amount_limit";
    rule.field_type = BS_SCHEMA_TYPE_I32;
    rule.op = "gt";
    rule.value = "50000";
    rule.layer = BS_GATE_LAYER_DEFAULT;

    const bs_gate_factory_t* f = bs_default_factory();
    if (!f) { printf("FAIL: factory is NULL\n"); return 1; }
    if (strcmp(f->name, "default") != 0) { printf("FAIL: factory name mismatch\n"); return 1; }
    if (f->layer != BS_GATE_LAYER_DEFAULT) { printf("FAIL: factory layer mismatch\n"); return 1; }

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

    bs_gate_rule_def_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.field_key = "approval_level";
    rule.field_type = BS_SCHEMA_TYPE_ENUM;
    rule.op = "eq";
    rule.value = "high";
    rule.layer = BS_GATE_LAYER_POLICY;
    rule.ai_hint = "审批等级，enum: low/medium/high";

    const bs_gate_factory_t* f = bs_policy_factory();
    bs_gate_node_t* node = NULL;
    int r = bs_gate_factory_produce(f, &rule, &node);
    if (r != 0) { printf("FAIL: policy produce returned %d\n", r); return 1; }
    if (!node) { printf("FAIL: policy node is NULL\n"); return 1; }

    if (strcmp(node->type, "bs_policy_attr") != 0) { printf("FAIL: type expected bs_policy_attr\n"); bs_gate_factory_free_node(node); return 1; }
    if (node->layer != BS_GATE_LAYER_POLICY) { printf("FAIL: layer not policy\n"); bs_gate_factory_free_node(node); return 1; }
    /* op eq + ai_hint contains "审批" → sub_category should be "enum_check" */
    if (strcmp(node->sub_category, "enum_check") != 0) {
        printf("FAIL: sub_category expected 'enum_check', got '%s'\n", node->sub_category);
        bs_gate_factory_free_node(node); return 1;
    }

    bs_gate_factory_free_node(node);
    printf("OK\n");
    return 0;
}

static int test_custom_factory(void)
{
    printf("  test_custom_factory ... ");

    bs_gate_rule_def_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.field_key = "server_port";
    rule.field_type = BS_SCHEMA_TYPE_I32;
    rule.op = "range";
    rule.value = "1024,65535";
    rule.layer = BS_GATE_LAYER_CUSTOM;

    const bs_gate_factory_t* f = bs_custom_factory();
    bs_gate_node_t* node = NULL;
    int r = bs_gate_factory_produce(f, &rule, &node);
    if (r != 0) { printf("FAIL: custom produce returned %d\n", r); return 1; }
    if (!node) { printf("FAIL: custom node is NULL\n"); return 1; }

    if (strcmp(node->type, "bs_custom_gate") != 0) { printf("FAIL: type expected bs_custom_gate\n"); bs_gate_factory_free_node(node); return 1; }
    if (node->layer != BS_GATE_LAYER_CUSTOM) { printf("FAIL: layer not custom\n"); bs_gate_factory_free_node(node); return 1; }

    bs_gate_factory_free_node(node);
    printf("OK\n");
    return 0;
}

static int test_infer_sub_category(void)
{
    printf("  test_infer_sub_category ... ");

    /* gt op → threshold */
    bs_gate_rule_def_t r1; memset(&r1, 0, sizeof(r1)); r1.op = "gt";
    const char* s1 = bs_gate_factory_infer_sub_category(&r1);
    if (strcmp(s1, "threshold") != 0) { printf("FAIL: gt op should infer threshold, got %s\n", s1); return 1; }

    /* eq + ai_hint="格式" → format */
    bs_gate_rule_def_t r2; memset(&r2, 0, sizeof(r2)); r2.op = "eq"; r2.ai_hint = "格式校验";
    const char* s2 = bs_gate_factory_infer_sub_category(&r2);
    if (strcmp(s2, "format") != 0) { printf("FAIL: eq+format should infer format, got %s\n", s2); return 1; }

    /* in op → enum_check */
    bs_gate_rule_def_t r3; memset(&r3, 0, sizeof(r3)); r3.op = "in";
    const char* s3 = bs_gate_factory_infer_sub_category(&r3);
    if (strcmp(s3, "enum_check") != 0) { printf("FAIL: in op should infer enum_check, got %s\n", s3); return 1; }

    /* ai_hint="告警" → alert */
    bs_gate_rule_def_t r4; memset(&r4, 0, sizeof(r4)); r4.ai_hint = "告警阈值";
    const char* s4 = bs_gate_factory_infer_sub_category(&r4);
    if (strcmp(s4, "alert") != 0) { printf("FAIL: 告警 should infer alert, got %s\n", s4); return 1; }

    printf("OK\n");
    return 0;
}

static int test_factory_by_layer(void)
{
    printf("  test_factory_by_layer ... ");

    const bs_gate_factory_t* fd = bs_gate_factory_by_layer(BS_GATE_LAYER_DEFAULT);
    if (!fd || strcmp(fd->name, "default") != 0) { printf("FAIL: default factory lookup\n"); return 1; }

    const bs_gate_factory_t* fp = bs_gate_factory_by_layer(BS_GATE_LAYER_POLICY);
    if (!fp || strcmp(fp->name, "policy") != 0) { printf("FAIL: policy factory lookup\n"); return 1; }

    const bs_gate_factory_t* fc = bs_gate_factory_by_layer(BS_GATE_LAYER_CUSTOM);
    if (!fc || strcmp(fc->name, "custom") != 0) { printf("FAIL: custom factory lookup\n"); return 1; }

    printf("OK\n");
    return 0;
}

static int test_factory_produce_nullargs(void)
{
    printf("  test_factory_produce_nullargs ... ");

    int r = bs_gate_factory_produce(NULL, NULL, NULL);
    if (r != -1) { printf("FAIL: null args should return -1\n"); return 1; }

    printf("OK\n");
    return 0;
}

int main(void)
{
    printf("gate_factory_test:\n");
    int fail = 0;
    fail += test_default_factory_threshold();
    fail += test_policy_factory();
    fail += test_custom_factory();
    fail += test_infer_sub_category();
    fail += test_factory_by_layer();
    fail += test_factory_produce_nullargs();
    printf("  %s\n", fail ? "FAILED" : "ALL PASS");
    return fail ? 1 : 0;
}
