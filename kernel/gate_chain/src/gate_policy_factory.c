/* gate_policy_factory: produces bs_policy_attr / bs_meta_rule nodes */
#include <bs/kernel/gate_chain/gate_factory.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bs_gate_node_t* policy_produce(const bs_gate_rule_def_t* rule)
{
    if (!rule) return NULL;

    bs_gate_node_t* node = (bs_gate_node_t*)calloc(1, sizeof(bs_gate_node_t));
    if (!node) return NULL;

    node->type     = strdup("bs_policy_attr");
    node->field_key = rule->field_key ? strdup(rule->field_key) : NULL;
    node->op       = rule->op ? strdup(rule->op) : NULL;
    node->value    = rule->value ? strdup(rule->value) : NULL;
    node->layer    = BS_GATE_LAYER_POLICY;

    const char* sub = bs_gate_factory_infer_sub_category(rule);
    const char* fk  = rule->field_key ? rule->field_key : "*";

    char sk_buf[512];
    snprintf(sk_buf, sizeof(sk_buf), "%s:%s:%d:%s",
             rule->scenario ? rule->scenario : "default",
             fk, (int)BS_GATE_LAYER_POLICY,
             sub ? sub : "policy");
    node->stable_key = strdup(sk_buf);

    node->sub_category = sub ? strdup(sub) : strdup("policy");
    node->id           = strdup(sk_buf);

    return node;
}

static bs_gate_factory_t s_policy_factory = {
    "policy",
    BS_GATE_LAYER_POLICY,
    policy_produce,
};

const bs_gate_factory_t* bs_policy_factory(void)
{
    return &s_policy_factory;
}
