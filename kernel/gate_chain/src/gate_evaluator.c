/* Gate evaluator: recursive DFS DAG traversal.
 * DAG version: evaluates from chain->root recursively. */
#include <bs/kernel/gate_chain/gate_evaluator.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Value comparison ──────────────────────────────────────────────── */

static int cmp_numeric(const char* a_str, const char* b_str)
{
    double a = strtod(a_str, NULL);
    double b = strtod(b_str, NULL);
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

static bool eval_condition(const bs_gate_node_t* node,
                            const char* field_value)
{
    if (!node || !node->op || !node->value || !field_value) return true;

    const char* op    = node->op;
    const char* thresh = node->value;

    if (strcmp(op, "eq") == 0)
        return strcmp(field_value, thresh) == 0;
    if (strcmp(op, "ne") == 0)
        return strcmp(field_value, thresh) != 0;
    if (strcmp(op, "gt") == 0)
        return cmp_numeric(field_value, thresh) > 0;
    if (strcmp(op, "lt") == 0)
        return cmp_numeric(field_value, thresh) < 0;
    if (strcmp(op, "gte") == 0)
        return cmp_numeric(field_value, thresh) >= 0;
    if (strcmp(op, "lte") == 0)
        return cmp_numeric(field_value, thresh) <= 0;
    if (strcmp(op, "in") == 0) {
        char* buf = strdup(thresh);
        if (!buf) return false;
        char* tok = strtok(buf, ",");
        bool found = false;
        while (tok) {
            if (strcmp(field_value, tok) == 0) { found = true; break; }
            tok = strtok(NULL, ",");
        }
        free(buf);
        return found;
    }
    if (strcmp(op, "range") == 0) {
        char* buf = strdup(thresh);
        if (!buf) return false;
        char* comma = strchr(buf, ',');
        if (!comma) { free(buf); return true; }
        *comma = '\0';
        double min = strtod(buf, NULL);
        double max = strtod(comma + 1, NULL);
        double val = strtod(field_value, NULL);
        free(buf);
        return val >= min && val <= max;
    }

    return true; /* unknown op: pass */
}

/* ── Forward declarations ──────────────────────────────────────────── */
static bool eval_node_recursive(const bs_gate_node_t* node,
                                 const bs_gate_eval_context_t* ctx,
                                 int* out_failed_layer,
                                 size_t* out_failed_depth);

static bool eval_do_nodes(const bs_gate_node_t* node,
                           const bs_gate_eval_context_t* ctx,
                           int* out_failed_layer,
                           size_t* out_failed_depth)
{
    if (!node || node->do_count == 0) return true;
    for (size_t i = 0; i < node->do_count; i++) {
        if (!eval_node_recursive(node->do_nodes[i], ctx,
                                  out_failed_layer, out_failed_depth))
            return false;
    }
    return true;
}

/* Recursive DFS node evaluation */
static bool eval_node_recursive(const bs_gate_node_t* node,
                                 const bs_gate_eval_context_t* ctx,
                                 int* out_failed_layer,
                                 size_t* out_failed_depth)
{
    if (!node) return true;

    const char* t = node->type ? node->type : "";

    /* ── Logic AND: all children must pass ── */
    if (strcmp(t, "bs_logic_and") == 0 || strcmp(t, "bs_gate_root") == 0) {
        for (size_t i = 0; i < node->child_count; i++) {
            if (!eval_node_recursive(node->children[i], ctx,
                                      out_failed_layer, out_failed_depth)) {
                /* Bubble up the failure info from child */
                return false;
            }
        }
        return true;
    }

    /* ── Logic OR: at least one child must pass ── */
    if (strcmp(t, "bs_logic_or") == 0) {
        if (node->child_count == 0) return true;
        for (size_t i = 0; i < node->child_count; i++) {
            if (eval_node_recursive(node->children[i], ctx,
                                     out_failed_layer, out_failed_depth))
                return true;
        }
        if (out_failed_layer) *out_failed_layer = node->layer;
        if (out_failed_depth) (*out_failed_depth)++;
        return false;
    }

    /* ── Condition / Meta-rule: evaluate leaf ── */
    if (strcmp(t, "bs_condition") == 0 || strcmp(t, "bs_meta_rule") == 0 ||
        strcmp(t, "bs_policy_attr") == 0 || strcmp(t, "bs_custom_gate") == 0) {

        /* Filter by field_key match—skip if field_key doesn't match ctx field */
        if (node->field_key && ctx->field_key &&
            strcmp(node->field_key, ctx->field_key) != 0)
            return true; /* Not relevant to this field, skip */

        /* Evaluate DO branch first? No: condition determines DO execution */
        if (!node->op || !node->value) {
            /* No condition (action-only node): execute and pass */
            return eval_do_nodes(node, ctx, out_failed_layer, out_failed_depth);
        }

        bool cond_pass = eval_condition(node, ctx->field_value);
        if (!cond_pass) {
            if (out_failed_layer) *out_failed_layer = node->layer;
            if (out_failed_depth) (*out_failed_depth)++;
            return false;
        }

        /* Condition passed: evaluate DO branch */
        return eval_do_nodes(node, ctx, out_failed_layer, out_failed_depth);
    }

    /* Default: unknown node type, pass through */
    return true;
}

/* ── Error message builder ─────────────────────────────────────────── */
static void build_error_msg(const bs_gate_node_t* failed_node,
                             const bs_gate_eval_context_t* ctx,
                             char** out_msg)
{
    if (!out_msg) return;
    char buf[512];
    if (failed_node) {
        snprintf(buf, sizeof(buf),
                 "Gate check failed: type=\"%s\" field=\"%s\" op=\"%s\" value=\"%s\" actual=\"%s\" layer=%d",
                 failed_node->type ? failed_node->type : "(any)",
                 failed_node->field_key ? failed_node->field_key : "(any)",
                 failed_node->op ? failed_node->op : "(any)",
                 failed_node->value ? failed_node->value : "(any)",
                 ctx->field_value ? ctx->field_value : "(null)",
                 failed_node->layer);
    } else {
        snprintf(buf, sizeof(buf),
                 "Gate check failed: unknown node");
    }
    *out_msg = strdup(buf);
}

int bs_gate_evaluator_evaluate(const bs_gate_chain_t* chain,
                                const bs_gate_eval_context_t* ctx,
                                bs_gate_eval_result_t* out)
{
    if (!chain || !ctx || !out) return -1;

    memset(out, 0, sizeof(*out));
    out->passed = true;

    /* Empty chain → pass */
    if (!chain->root) return 0;

    /* Recursive DFS evaluation */
    int failed_layer = -1;
    size_t failed_depth = 0;
    bool passed = eval_node_recursive(chain->root, ctx,
                                       &failed_layer, &failed_depth);

    out->passed = passed;
    if (!passed) {
        if (failed_layer >= 0)
            out->failed_layer = (size_t)failed_layer;
        out->failed_node_index = failed_depth;
        /* Attempt to find the failing node for better error message */
        /* For now, build generic error */
        build_error_msg(NULL, ctx, &out->error_message);
    }

    return 0;
}

void bs_gate_eval_result_free(bs_gate_eval_result_t* result)
{
    if (!result) return;
    free(result->error_message);
    result->error_message = NULL;
}
