#include "gate_rule_loader.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

/* ── Minimal JSON parser for gate_rule_def.json ─────────────────────── */

/* Forward declarations */
static std::string json_parse_string(const char*& p);
static std::string json_parse_value(const char*& p);
static void        json_skip_value(const char*& p);
static void        json_skip_spaces(const char*& p) {
    while (p && *p && (unsigned char)*p <= 32) p++;
}

static std::string json_parse_string(const char*& p) {
    json_skip_spaces(p);
    if (!p || *p != '"') return {};
    p++; /* skip opening quote */
    std::string s;
    while (*p) {
        if (*p == '"') { p++; return s; }
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
                case '"':  s += '"';  break;
                case '\\': s += '\\'; break;
                case 'n':  s += '\n'; break;
                case 'r':  s += '\r'; break;
                case 't':  s += '\t'; break;
                default:   s += *p;   break;
            }
        } else {
            s += *p;
        }
        p++;
    }
    return s;
}

static std::string json_parse_value(const char*& p) {
    json_skip_spaces(p);
    if (!p || !*p) return {};
    if (*p == '"') return json_parse_string(p);
    if (*p == '{' || *p == '[') {
        char close = (*p == '{') ? '}' : ']';
        p++;
        int depth = 1;
        while (*p && depth > 0) {
            if (*p == '"') { json_parse_string(p); continue; }
            if (*p == '{' || *p == '[') depth++;
            else if (*p == close) depth--;
            p++;
        }
        return {};
    }
    /* number / true / false / null */
    const char* start = p;
    while (*p && !isspace((unsigned char)*p) && *p != ',' && *p != '}' && *p != ']') p++;
    return std::string(start, p);
}

static void json_skip_value(const char*& p) {
    json_parse_value(p);
}

/* Find a string value by key in a JSON object (starts after '{') */
static std::string json_find_string(const char*& p, const char* key) {
    json_skip_spaces(p);
    while (p && *p && *p != '}') {
        std::string k = json_parse_string(p);
        if (k.empty()) break;
        json_skip_spaces(p);
        if (*p != ':') break;
        p++; /* skip ':' */
        if (k == key) {
            return json_parse_value(p);
        }
        json_skip_value(p);
        json_skip_spaces(p);
        if (*p == ',') p++;
    }
    return {};
}

static bool json_find_bool(const char*& p, const char* key, bool def) {
    std::string v = json_find_string(p, key);
    if (v.empty()) return def;
    return v == "true";
}

static int json_find_int(const char*& p, const char* key, int def) {
    std::string v = json_find_string(p, key);
    if (v.empty()) return def;
    return std::atoi(v.c_str());
}

/* ── Schema type mapping ──────────────────────────────────────────────── */

bs_schema_type_t GateRuleDef::to_schema_type() const {
    if (field_type_str == "INT64")  return BS_SCHEMA_TYPE_I64;
    if (field_type_str == "INT32")  return BS_SCHEMA_TYPE_I32;
    if (field_type_str == "FLOAT64") return BS_SCHEMA_TYPE_F64;
    if (field_type_str == "BOOL")   return BS_SCHEMA_TYPE_BOOL;
    if (field_type_str == "STRING") return BS_SCHEMA_TYPE_STR;
    /* fallback */
    return BS_SCHEMA_TYPE_STR;
}

bs_gate_layer_t GateRuleDef::to_gate_layer() const {
    if (layer == 1) return BS_GATE_LAYER_POLICY;
    if (layer == 2) return BS_GATE_LAYER_CUSTOM;
    return BS_GATE_LAYER_DEFAULT;
}

/* ── Load gate rules from JSON file ────────────────────────────────────── */

std::vector<GateRuleDef> load_gate_rules(const std::string& file_path) {
    std::vector<GateRuleDef> rules;

    std::ifstream in(file_path, std::ios::in | std::ios::binary);
    if (!in) {
        std::cerr << "  [ERROR] Cannot open gate rule file: " << file_path << "\n";
        return rules;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();

    const char* p = content.c_str();
    json_skip_spaces(p);

    /* Expect JSON array */
    if (!p || *p != '[') {
        std::cerr << "  [ERROR] Expected JSON array in gate rule file\n";
        return rules;
    }
    p++;

    while (p && *p) {
        json_skip_spaces(p);
        if (*p == ']') break;
        if (*p != '{') { p++; continue; }
        p++; /* skip { */

        GateRuleDef rule;
        rule.enabled = true;

        /* Single pass: parse all key-value pairs within this object */
        while (p && *p) {
            json_skip_spaces(p);
            if (*p == '}') break;

            std::string k = json_parse_string(p);
            if (k.empty()) break;

            json_skip_spaces(p);
            if (*p != ':') break;
            p++; /* skip ':' */

            if (k == "field_key") {
                rule.field_key = json_parse_string(p);
            } else if (k == "field_type") {
                rule.field_type_str = json_parse_string(p);
            } else if (k == "op") {
                rule.op = json_parse_string(p);
            } else if (k == "value") {
                rule.value = json_parse_value(p);
            } else if (k == "scenario") {
                rule.scenario = json_parse_string(p);
            } else if (k == "sub_category") {
                rule.sub_category = json_parse_string(p);
            } else if (k == "stable_key") {
                rule.stable_key = json_parse_string(p);
            } else if (k == "error_hint") {
                rule.error_hint = json_parse_string(p);
            } else if (k == "layer") {
                std::string vs = json_parse_value(p);
                rule.layer = std::atoi(vs.c_str());
            } else if (k == "enabled") {
                std::string vs = json_parse_value(p);
                rule.enabled = (vs == "true");
            } else {
                json_skip_value(p);
            }

            json_skip_spaces(p);
            if (*p == ',') p++;
        }

        json_skip_spaces(p);
        if (*p == '}') p++;

        if (!rule.enabled) continue;
        if (rule.field_key.empty()) continue;

        rules.push_back(std::move(rule));

        json_skip_spaces(p);
        if (*p == ',') p++;
    }

    return rules;
}

/* ── Build gate chain ──────────────────────────────────────────────────── */

int build_gate_chain(bs_gate_chain_t* chain,
                     const std::vector<GateRuleDef>& rules,
                     const std::string& biz_id) {
    if (!chain) return -1;

    int success_count = 0;
    int skip_count = 0;

    for (size_t i = 0; i < rules.size(); i++) {
        const GateRuleDef& r = rules[i];

        /* Determine which factory to use based on layer */
        const bs_gate_factory_t* factory = nullptr;
        if (r.layer == 1) {
            factory = bs_policy_factory();
        } else if (r.layer == 2) {
            factory = bs_custom_factory();
        } else {
            factory = bs_default_factory();
        }

        if (!factory) {
            std::cerr << "  [ERROR] No factory for layer " << r.layer << " on rule "
                      << r.stable_key << "\n";
            skip_count++;
            continue;
        }

        /* Build bs_gate_rule_def_t for the factory */
        bs_gate_rule_def_t rule_def;
        rule_def.field_key  = r.field_key.c_str();
        rule_def.field_type = r.to_schema_type();
        rule_def.op         = r.op.c_str();
        rule_def.value      = r.value.c_str();
        rule_def.scenario   = r.scenario.c_str();
        rule_def.layer      = r.to_gate_layer();
        rule_def.ai_hint    = r.error_hint.c_str();

        /* Produce gate node via factory */
        bs_gate_node_t* node = nullptr;
        int rc = bs_gate_factory_produce(factory, &rule_def, &node);
        if (rc != 0 || !node) {
            std::cerr << "  [ERROR] Factory produce failed for: " << r.stable_key
                      << " (rc=" << rc << ")\n";
            skip_count++;
            continue;
        }

        /* Set stable_key on the produced node (factory may not set it) */
        if (!node->stable_key && !r.stable_key.empty()) {
            node->stable_key = strdup(r.stable_key.c_str());
        }
        if (!node->sub_category && !r.sub_category.empty()) {
            node->sub_category = strdup(r.sub_category.c_str());
        }
        /* Set domain/entity from stable_key: format <domain>:<entity>:... */
        if (!node->domain && !r.stable_key.empty()) {
            size_t first_colon = r.stable_key.find(':');
            if (first_colon != std::string::npos) {
                std::string domain = r.stable_key.substr(0, first_colon);
                node->domain = strdup(domain.c_str());
                size_t second_colon = r.stable_key.find(':', first_colon + 1);
                if (second_colon != std::string::npos) {
                    std::string entity = r.stable_key.substr(first_colon + 1,
                                                             second_colon - first_colon - 1);
                    node->entity = strdup(entity.c_str());
                }
            }
        }

        /* Upsert into gate chain */
        bs_gate_node_t* upserted = bs_gate_chain_upsert_node(chain, node);
        if (!upserted) {
            std::cerr << "  [ERROR] Gate chain upsert failed for: " << r.stable_key << "\n";
            bs_gate_factory_free_node(node);
            skip_count++;
            continue;
        }

        success_count++;
        bs_gate_factory_free_node(node);
    }

    std::cout << "  Gate chain: " << success_count << " rules compiled, "
              << skip_count << " skipped\n";
    return skip_count > 0 ? -1 : 0;
}

/* ── Print gate chain summary ──────────────────────────────────────────── */

void print_gate_chain_summary(const bs_gate_chain_t* chain,
                               const std::string& biz_id) {
    if (!chain) {
        std::cout << "  Gate chain: (null)\n";
        return;
    }

    size_t node_count = bs_gate_chain_node_count(chain);
    std::cout << "  Gate chain version: " << (chain->version ? chain->version : "1.0") << "\n";
    std::cout << "  Total DAG nodes: " << node_count << "\n";

    /* Count by layer */
    int layer_counts[3] = {0, 0, 0};
    if (chain->map && chain->map->slots) {
        for (size_t i = 0; i < chain->map->capacity; i++) {
            if (chain->map->slots[i].node_ptr) {
                int l = chain->map->slots[i].node_ptr->layer;
                if (l >= 0 && l < 3) layer_counts[l]++;
            }
        }
    }
    std::cout << "    DEFAULT layer: " << layer_counts[0] << " nodes\n";
    std::cout << "    POLICY  layer: " << layer_counts[1] << " nodes\n";
    std::cout << "    CUSTOM  layer: " << layer_counts[2] << " nodes\n";
}
