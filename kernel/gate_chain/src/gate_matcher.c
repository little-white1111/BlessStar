/* Gate semantic matcher: matches schema fields to gate nodes via 4-tier priority */
#include <bs/kernel/gate_chain/gate_matcher.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Match priority levels */
typedef enum {
    MATCH_EXACT      = 0, /* field_key == schema.field.name */
    MATCH_TYPE       = 1, /* by schema.field.type */
    MATCH_WILDCARD   = 2, /* field_key == "*" */
    MATCH_SEMANTIC   = 3, /* biz_index ai_hint semantic */
    MATCH_NONE       = 99
} match_priority_t;

static const char* schema_type_to_op_type(bs_schema_type_t t)
{
    switch (t) {
    case BS_SCHEMA_TYPE_I32:
    case BS_SCHEMA_TYPE_I64:
    case BS_SCHEMA_TYPE_F64:
        return "numeric";
    case BS_SCHEMA_TYPE_STR:
        return "string";
    case BS_SCHEMA_TYPE_BOOL:
        return "bool";
    case BS_SCHEMA_TYPE_ENUM:
        return "enum";
    default:
        return "any";
    }
}

/* Check if op matches field type */
static bool op_matches_type(const char* op, const char* type_family)
{
    if (!op || !type_family) return true;
    if (strcmp(type_family, "numeric") == 0) {
        return (strcmp(op, "gt") == 0 || strcmp(op, "lt") == 0 ||
                strcmp(op, "gte") == 0 || strcmp(op, "lte") == 0 ||
                strcmp(op, "eq") == 0 || strcmp(op, "ne") == 0 ||
                strcmp(op, "range") == 0);
    }
    if (strcmp(type_family, "string") == 0) {
        return (strcmp(op, "eq") == 0 || strcmp(op, "ne") == 0 ||
                strcmp(op, "in") == 0 || strcmp(op, "match") == 0);
    }
    if (strcmp(type_family, "bool") == 0) {
        return (strcmp(op, "eq") == 0 || strcmp(op, "ne") == 0);
    }
    if (strcmp(type_family, "enum") == 0) {
        return (strcmp(op, "eq") == 0 || strcmp(op, "ne") == 0 ||
                strcmp(op, "in") == 0);
    }
    return true;
}

/* Check if ai_hint semantically overlaps with sub_category */
static bool hint_matches_sub(const char* ai_hint, const char* sub_category)
{
    if (!ai_hint || !sub_category) return false;

    if (strcmp(sub_category, "threshold") == 0) {
        return (strstr(ai_hint, "阈值") || strstr(ai_hint, "threshold") ||
                strstr(ai_hint, "限制") || strstr(ai_hint, "limit") ||
                strstr(ai_hint, "max") || strstr(ai_hint, "min") ||
                strstr(ai_hint, "上限") || strstr(ai_hint, "下限"));
    }
    if (strcmp(sub_category, "approval") == 0) {
        return (strstr(ai_hint, "审批") || strstr(ai_hint, "approve") ||
                strstr(ai_hint, "审核"));
    }
    if (strcmp(sub_category, "alert") == 0) {
        return (strstr(ai_hint, "告警") || strstr(ai_hint, "alert") ||
                strstr(ai_hint, "warn") || strstr(ai_hint, "提醒"));
    }
    if (strcmp(sub_category, "enum_check") == 0) {
        return (strstr(ai_hint, "枚举") || strstr(ai_hint, "enum") ||
                strstr(ai_hint, "选项"));
    }
    if (strcmp(sub_category, "format") == 0) {
        return (strstr(ai_hint, "格式") || strstr(ai_hint, "format") ||
                strstr(ai_hint, "模式") || strstr(ai_hint, "pattern"));
    }
    return false;
}

int bs_gate_matcher_match(const bs_schema_field_def_t* field,
                           const char* scenario,
                           bs_gate_match_result_t* out)
{
    if (!field || !out) return -1;

    memset(out, 0, sizeof(*out));

    /* For now, return an empty match set.
     * Full implementation would traverse a gate registry (loaded at init)
     * and filter nodes by scenario + priority matching. */
    (void)scenario;

    /* Produce at most 8 nodes */
    size_t cap = 8;
    out->nodes = (bs_gate_node_t*)calloc(cap, sizeof(bs_gate_node_t));
    if (!out->nodes) return -1;

    out->node_count = 0;
    out->confidence = 0.0;

    return 0;
}

void bs_gate_match_result_free(bs_gate_match_result_t* result)
{
    if (!result) return;
    if (result->nodes) {
        for (size_t i = 0; i < result->node_count; i++) {
            bs_gate_node_t* n = &result->nodes[i];
            free(n->type); free(n->id); free(n->field_key);
            free(n->op); free(n->value);
            free(n->stable_key); free(n->sub_category);
            free(n->domain); free(n->entity);
            /* Match result nodes are shallow copies; no child/do tree to free */
        }
        free(result->nodes);
    }
}
