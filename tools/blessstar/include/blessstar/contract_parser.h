#ifndef BLESSSTAR_CONTRACT_PARSER_H
#define BLESSSTAR_CONTRACT_PARSER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BS_CONTRACT_MAX_FIELDS 64
#define BS_CONTRACT_MAX_GATES 32
#define BS_CONTRACT_MAX_EFFECTS 32
#define BS_CONTRACT_MAX_DEPS 16
#define BS_CONTRACT_MAX_KEY_LEN 128
#define BS_CONTRACT_MAX_STR_LEN 256

typedef enum {
    BS_FIELD_TYPE_INT32, BS_FIELD_TYPE_INT64, BS_FIELD_TYPE_STRING,
    BS_FIELD_TYPE_DOUBLE, BS_FIELD_TYPE_BOOL, BS_FIELD_TYPE_FILE
} bs_contract_field_type_t;

typedef struct {
    char key[BS_CONTRACT_MAX_KEY_LEN];
    bs_contract_field_type_t type;
    char default_str[BS_CONTRACT_MAX_STR_LEN];
    char description[BS_CONTRACT_MAX_STR_LEN];
    int required;
} bs_contract_field_t;

typedef struct {
    char id[BS_CONTRACT_MAX_KEY_LEN];
    char description[BS_CONTRACT_MAX_STR_LEN];
    char field[BS_CONTRACT_MAX_KEY_LEN];
    char rule[BS_CONTRACT_MAX_STR_LEN];
    char layer[32];
    char sub_category[32];
    char scenario[64];
    char dependencies[BS_CONTRACT_MAX_DEPS][BS_CONTRACT_MAX_KEY_LEN];
    size_t dependencies_count;
} bs_contract_gate_t;

typedef struct {
    char trigger[BS_CONTRACT_MAX_KEY_LEN];
    char action[BS_CONTRACT_MAX_STR_LEN];
    int async;
} bs_contract_effect_t;

typedef struct {
    char biz_id[BS_CONTRACT_MAX_KEY_LEN];
    char display_name[BS_CONTRACT_MAX_STR_LEN];
    char version[32];
    char sdk_version[32];
    char description[BS_CONTRACT_MAX_STR_LEN];
    
    bs_contract_field_t fields[BS_CONTRACT_MAX_FIELDS];
    size_t fields_count;
    
    char dependencies[BS_CONTRACT_MAX_DEPS][BS_CONTRACT_MAX_KEY_LEN];
    size_t dependencies_count;
    
    bs_contract_gate_t gates[BS_CONTRACT_MAX_GATES];
    size_t gates_count;
    
    bs_contract_effect_t effects[BS_CONTRACT_MAX_EFFECTS];
    size_t effects_count;
} bs_contract_t;

/**
 * Parse a contract.yaml file into bs_contract_t.
 * Returns 0 on success, -1 on parse error.
 * Handles YAML-like format: simple key: value pairs, list items with - prefix,
 * nested sections via indentation.
 */
int bs_contract_parse(const char* yaml_text, bs_contract_t* out);

/**
 * Parse from file.
 */
int bs_contract_parse_file(const char* file_path, bs_contract_t* out);

/**
 * Free allocated memory (if any).
 */
void bs_contract_destroy(bs_contract_t* contract);

/**
 * Get field type from string name.
 */
bs_contract_field_type_t bs_contract_field_type_from_str(const char* type_str);

/**
 * Get type string for Mustache template (uppercase, e.g. "INT32").
 */
const char* bs_contract_field_type_to_upper(bs_contract_field_type_t t);

#ifdef __cplusplus
}
#endif

#endif
