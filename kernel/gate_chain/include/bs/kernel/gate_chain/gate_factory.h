#ifndef BS_GATE_FACTORY_H
#define BS_GATE_FACTORY_H

#include <bs/kernel/gate_chain/gate_chain_types.h>
#include <bs/kernel/schema/schema_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Gate rule definition (input to factory) ───────────────────────── */
typedef struct bs_gate_rule_def {
    const char*      field_key;
    bs_schema_type_t field_type;
    const char*      op;
    const char*      value;
    const char*      scenario;
    bs_gate_layer_t  layer;
    const char*      ai_hint;
} bs_gate_rule_def_t;

/* ── Factory produce function pointer ──────────────────────────────── */
typedef bs_gate_node_t* (*gate_factory_produce_fn)(const bs_gate_rule_def_t* rule);

/* ── Gate factory descriptor ───────────────────────────────────────── */
typedef struct bs_gate_factory {
    const char*            name;
    bs_gate_layer_t        layer;
    gate_factory_produce_fn produce;
} bs_gate_factory_t;

/* ── Produce a gate node via factory ────────────────────────────────── */
int  bs_gate_factory_produce(const bs_gate_factory_t* factory,
                              const bs_gate_rule_def_t* rule,
                              bs_gate_node_t** out);
void bs_gate_factory_free_node(bs_gate_node_t* node);

/* ── Built-in factory singletons ───────────────────────────────────── */
const bs_gate_factory_t* bs_default_factory(void);
const bs_gate_factory_t* bs_policy_factory(void);
const bs_gate_factory_t* bs_custom_factory(void);

/* ── Lookup factory by layer ───────────────────────────────────────── */
const bs_gate_factory_t* bs_gate_factory_by_layer(bs_gate_layer_t layer);

/* ── Infer sub_category from rule semantics ────────────────────────── */
const char* bs_gate_factory_infer_sub_category(const bs_gate_rule_def_t* rule);

#ifdef __cplusplus
}
#endif

#endif /* BS_GATE_FACTORY_H */
