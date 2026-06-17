/* gate_default_factory: produces bs_condition / bs_gate_default nodes */
#include <bs/kernel/gate_chain/gate_factory.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Shared utilities across all factories ──────────────────────────── */

const char* bs_gate_factory_infer_sub_category(const bs_gate_rule_def_t* rule)
{
    if (!rule) return "threshold";

    const char* op = rule->op;
    const char* ai = rule->ai_hint;

    if (op) {
        if (strcmp(op, "eq") == 0 || strcmp(op, "ne") == 0) {
            /* Value equality suggests enum/format check */
            if (ai && (strstr(ai, "enum") || strstr(ai, "枚举") ||
                       strstr(ai, "format") || strstr(ai, "格式")))
                return "enum_check";
            return "format";
        }
        if (strcmp(op, "gt") == 0 || strcmp(op, "lt") == 0 ||
            strcmp(op, "gte") == 0 || strcmp(op, "lte") == 0)
            return "threshold";
        if (strcmp(op, "in") == 0)
            return "enum_check";
        if (strcmp(op, "range") == 0)
            return "threshold";
        if (strcmp(op, "match") == 0)
            return "format";
    }

    /* Infer from ai_hint keywords */
    if (ai) {
        if (strstr(ai, "阈值") || strstr(ai, "threshold") ||
            strstr(ai, "限制") || strstr(ai, "limit") ||
            strstr(ai, "上限") || strstr(ai, "下限") ||
            strstr(ai, "max") || strstr(ai, "min"))
            return "threshold";
        if (strstr(ai, "审批") || strstr(ai, "approve") ||
            strstr(ai, "approval"))
            return "approval";
        if (strstr(ai, "告警") || strstr(ai, "alert") ||
            strstr(ai, "warn"))
            return "alert";
        if (strstr(ai, "枚举") || strstr(ai, "enum"))
            return "enum_check";
    }

    return "threshold";
}

const bs_gate_factory_t* bs_gate_factory_by_layer(bs_gate_layer_t layer)
{
    switch (layer) {
    case BS_GATE_LAYER_DEFAULT: return bs_default_factory();
    case BS_GATE_LAYER_POLICY:  return bs_policy_factory();
    case BS_GATE_LAYER_CUSTOM:  return bs_custom_factory();
    default:                    return bs_default_factory();
    }
}

int bs_gate_factory_produce(const bs_gate_factory_t* factory,
                              const bs_gate_rule_def_t* rule,
                              bs_gate_node_t** out)
{
    if (!factory || !rule || !out) return -1;
    if (!factory->produce) return -1;
    *out = factory->produce(rule);
    return (*out) ? 0 : -1;
}

void bs_gate_factory_free_node(bs_gate_node_t* node)
{
    if (!node) return;
    free(node->type);
    free(node->id);
    free(node->field_key);
    free(node->op);
    free(node->value);
    free(node->stable_key);
    free(node->sub_category);
    free(node->domain);
    free(node->entity);
    if (node->child_ids) {
        for (size_t j = 0; j < node->child_count; j++)
            free(node->child_ids[j]);
        free(node->child_ids);
    }
    if (node->do_ids) {
        for (size_t j = 0; j < node->do_count; j++)
            free(node->do_ids[j]);
        free(node->do_ids);
    }
    free(node);
}

static bs_gate_node_t* default_produce(const bs_gate_rule_def_t* rule)
{
    if (!rule) return NULL;

    bs_gate_node_t* node = (bs_gate_node_t*)calloc(1, sizeof(bs_gate_node_t));
    if (!node) return NULL;

    node->type     = strdup("bs_condition");
    node->field_key = rule->field_key ? strdup(rule->field_key) : NULL;
    node->op       = rule->op ? strdup(rule->op) : NULL;
    node->value    = rule->value ? strdup(rule->value) : NULL;
    node->layer    = BS_GATE_LAYER_DEFAULT;

    /* Generate stable_key */
    const char* sub = bs_gate_factory_infer_sub_category(rule);
    const char* fk  = rule->field_key ? rule->field_key : "*";
    char sk_buf[512];
    snprintf(sk_buf, sizeof(sk_buf), "%s:%s:%d:%s",
             rule->scenario ? rule->scenario : "default",
             fk, (int)BS_GATE_LAYER_DEFAULT,
             sub ? sub : "threshold");
    node->stable_key = strdup(sk_buf);

    node->sub_category = sub ? strdup(sub) : strdup("threshold");

    /* Generate ID from stable_key */
    node->id = strdup(sk_buf);

    return node;
}

static bs_gate_factory_t s_default_factory = {
    "default",
    BS_GATE_LAYER_DEFAULT,
    default_produce,
};

const bs_gate_factory_t* bs_default_factory(void)
{
    return &s_default_factory;
}
