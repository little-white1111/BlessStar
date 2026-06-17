#ifndef BS_GATE_MATCHER_H
#define BS_GATE_MATCHER_H

#include <bs/kernel/gate_chain/gate_chain_types.h>
#include <bs/kernel/schema/schema_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Match result ──────────────────────────────────────────────────── */
typedef struct bs_gate_match_result {
    bs_gate_node_t* nodes;
    size_t          node_count;
    double          confidence;
} bs_gate_match_result_t;

/* ── Match gates for a schema field under a scenario ────────────────── */
int  bs_gate_matcher_match(const bs_schema_field_def_t* field,
                            const char* scenario,
                            bs_gate_match_result_t* out);
void bs_gate_match_result_free(bs_gate_match_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* BS_GATE_MATCHER_H */
