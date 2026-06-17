/* gate_custom_factory: produces bs_custom_gate / bs_logic_and / bs_logic_or nodes */
#include <bs/kernel/gate_chain/gate_factory.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bs_gate_node_t* custom_produce(const bs_gate_rule_def_t* rule)
{
    if (!rule) return NULL;

    bs_gate_node_t* node = (bs_gate_node_t*)calloc(1, sizeof(bs_gate_node_t));
    if (!node) return NULL;

    node->type     = strdup("bs_custom_gate");
    node->field_key = rule->field_key ? strdup(rule->field_key) : NULL;
    node->op       = rule->op ? strdup(rule->op) : NULL;
    node->value    = rule->value ? strdup(rule->value) : NULL;
    node->layer    = BS_GATE_LAYER_CUSTOM;

    const char* sub = bs_gate_factory_infer_sub_category(rule);
    const char* fk  = rule->field_key ? rule->field_key : "*";

    char sk_buf[512];
    snprintf(sk_buf, sizeof(sk_buf), "%s:%s:%d:%s",
             rule->scenario ? rule->scenario : "default",
             fk, (int)BS_GATE_LAYER_CUSTOM,
             sub ? sub : "custom");
    node->stable_key = strdup(sk_buf);

    node->sub_category = sub ? strdup(sub) : strdup("custom");
    node->id           = strdup(sk_buf);

    return node;
}

static bs_gate_factory_t s_custom_factory = {
    "custom",
    BS_GATE_LAYER_CUSTOM,
    custom_produce,
};

const bs_gate_factory_t* bs_custom_factory(void)
{
    return &s_custom_factory;
}
