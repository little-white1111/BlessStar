/* Gate evaluator: evaluate a gate chain against a single field value */
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
        /* Threshold is comma-separated list */
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
        /* Threshold is "min,max" */
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

int bs_gate_evaluator_evaluate(const bs_gate_chain_t* chain,
                                const bs_gate_eval_context_t* ctx,
                                bs_gate_eval_result_t* out)
{
    if (!chain || !ctx || !out) return -1;

    memset(out, 0, sizeof(*out));
    out->passed = true;

    if (chain->node_count == 0) return 0;

    /* Evaluate nodes in layer order: DEFAULT → POLICY → CUSTOM */
    for (int layer = 0; layer < BS_GATE_LAYER_COUNT; layer++) {
        for (size_t i = 0; i < chain->node_count; i++) {
            const bs_gate_node_t* n = &chain->nodes[i];
            if (n->layer != layer) continue;

            /* Filter by field_key match */
            if (n->field_key && ctx->field_key &&
                strcmp(n->field_key, ctx->field_key) != 0)
                continue;

            /* Skip logic nodes without conditions */
            if (!n->op || !n->value) continue;

            if (!eval_condition(n, ctx->field_value)) {
                out->passed = false;
                out->failed_layer = (size_t)layer;
                out->failed_node_index = i;

                char err_buf[512];
                snprintf(err_buf, sizeof(err_buf),
                         "Gate check failed: field=\"%s\" op=\"%s\" value=\"%s\" actual=\"%s\" layer=%d",
                         n->field_key ? n->field_key : "(any)",
                         n->op, n->value,
                         ctx->field_value ? ctx->field_value : "(null)",
                         layer);
                out->error_message = strdup(err_buf);
                return 0;
            }
        }
    }

    out->passed = true;
    return 0;
}

void bs_gate_eval_result_free(bs_gate_eval_result_t* result)
{
    if (!result) return;
    free(result->error_message);
    result->error_message = NULL;
}
