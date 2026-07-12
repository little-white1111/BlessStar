#ifndef BIZ_BOOTSTRAP_GATE_RULE_LOADER_H
#define BIZ_BOOTSTRAP_GATE_RULE_LOADER_H

#include <bs/kernel/gate_chain/gate_factory.h>
#include <bs/kernel/gate_chain/gate_chain_types.h>

#include <string>
#include <vector>

/**
 * Parsed gate rule definition from JSON.
 * Extends bs_gate_rule_def_t with JSON-native fields.
 */
struct GateRuleDef {
    std::string field_key;
    std::string field_type_str;   // "INT64", "INT32", "STRING", "BOOL", "FLOAT64"
    std::string op;
    std::string value;
    std::string scenario;
    int         layer;
    std::string sub_category;
    std::string stable_key;
    std::string error_hint;
    bool        enabled;

    bs_schema_type_t to_schema_type() const;
    bs_gate_layer_t  to_gate_layer() const;
};

/**
 * Load gate rules from a gate_rule_def.json file.
 * Returns parsed rules (only enabled=true rules are returned).
 */
std::vector<GateRuleDef> load_gate_rules(const std::string& file_path);

/**
 * Build gate chain from parsed rules.
 * @param chain     Gate chain to populate (must be created via bs_gate_chain_create())
 * @param rules     Parsed rule definitions
 * @param biz_id    Business system ID (for logging)
 * @return 0 on success, -1 on failure
 */
int build_gate_chain(bs_gate_chain_t* chain,
                     const std::vector<GateRuleDef>& rules,
                     const std::string& biz_id);

/**
 * Print gate chain summary.
 */
void print_gate_chain_summary(const bs_gate_chain_t* chain,
                               const std::string& biz_id);

#endif // BIZ_BOOTSTRAP_GATE_RULE_LOADER_H
