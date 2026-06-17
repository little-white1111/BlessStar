#ifndef BS_GATE_EVALUATOR_H
#define BS_GATE_EVALUATOR_H

#include <bs/kernel/gate_chain/gate_chain_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Evaluation context ────────────────────────────────────────────── */
typedef struct bs_gate_eval_context {
    const char* field_key;
    const char* field_value;
    void*       user_data;
} bs_gate_eval_context_t;

/* ── Evaluation result ─────────────────────────────────────────────── */
typedef struct bs_gate_eval_result {
    bool   passed;
    size_t failed_layer;
    size_t failed_node_index;
    char*  error_message;
} bs_gate_eval_result_t;

/* ── Evaluate a gate chain against a single field value ─────────────── */
int  bs_gate_evaluator_evaluate(const bs_gate_chain_t* chain,
                                 const bs_gate_eval_context_t* ctx,
                                 bs_gate_eval_result_t* out);
void bs_gate_eval_result_free(bs_gate_eval_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* BS_GATE_EVALUATOR_H */
