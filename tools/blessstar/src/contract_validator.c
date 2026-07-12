#include "blessstar/contract_validator.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/**
 * Check if a field type string is valid.
 */
static int is_valid_type_str(const char* type_str) {
    if (!type_str) return 0;

    char lower[32];
    size_t i;
    for (i = 0; i < sizeof(lower) - 1 && type_str[i]; i++) {
        lower[i] = (char)tolower((unsigned char)type_str[i]);
    }
    lower[i] = '\0';

    return (strcmp(lower, "int32") == 0 || strcmp(lower, "int64") == 0 ||
            strcmp(lower, "string") == 0 || strcmp(lower, "double") == 0 ||
            strcmp(lower, "bool") == 0 || strcmp(lower, "file") == 0);
}

/**
 * Get the string representation of a field type for error messages.
 */
static const char* field_type_to_str(bs_contract_field_type_t t) {
    switch (t) {
        case BS_FIELD_TYPE_INT32:  return "int32";
        case BS_FIELD_TYPE_INT64:  return "int64";
        case BS_FIELD_TYPE_STRING: return "string";
        case BS_FIELD_TYPE_DOUBLE: return "double";
        case BS_FIELD_TYPE_BOOL:   return "bool";
        case BS_FIELD_TYPE_FILE:   return "file";
        default:                   return "unknown";
    }
}

int bs_contract_validate(const bs_contract_t* contract, char* error_buf, size_t error_buf_size) {
    if (!contract) {
        if (error_buf && error_buf_size > 0) {
            snprintf(error_buf, error_buf_size, "contract pointer is NULL");
        }
        return 1;
    }

    int errors = 0;
    size_t buf_used = 0;

/* Helper macro: write formatted error to buffer */
#define APPEND_ERROR(fmt, ...) do { \
    int written = snprintf(error_buf + buf_used, \
                           (buf_used < error_buf_size) ? (error_buf_size - buf_used) : 0, \
                           fmt "\n", ##__VA_ARGS__); \
    if (written > 0) buf_used += (size_t)written; \
    if (buf_used > error_buf_size) buf_used = error_buf_size; \
    errors++; \
} while (0)

    /* Validate biz_id */
    if (contract->biz_id[0] == '\0') {
        APPEND_ERROR("biz_id must be non-empty");
    }

    /* Validate display_name */
    if (contract->display_name[0] == '\0') {
        APPEND_ERROR("display_name must be non-empty");
    }

    /* Validate fields */
    for (size_t i = 0; i < contract->fields_count; i++) {
        const bs_contract_field_t* f = &contract->fields[i];

        if (f->key[0] == '\0') {
            APPEND_ERROR("field[%zu]: key must be non-empty", i);
        }

        if (!is_valid_type_str(field_type_to_str(f->type))) {
            APPEND_ERROR("field '%s': invalid type", f->key[0] ? f->key : "(unnamed)");
        }

        /* Check for duplicate keys */
        for (size_t j = 0; j < i; j++) {
            if (strcmp(contract->fields[j].key, f->key) == 0 && f->key[0] != '\0') {
                APPEND_ERROR("field '%s': duplicate key", f->key);
            }
        }
    }

    /* Validate gates */
    for (size_t i = 0; i < contract->gates_count; i++) {
        const bs_contract_gate_t* g = &contract->gates[i];

        if (g->id[0] == '\0') {
            APPEND_ERROR("gate[%zu]: id must be non-empty", i);
        }

        /* Check gate field references an existing field */
        if (g->field[0] != '\0') {
            int found = 0;
            for (size_t j = 0; j < contract->fields_count; j++) {
                if (strcmp(contract->fields[j].key, g->field) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                APPEND_ERROR("gate '%s': references unknown field '%s'", g->id, g->field);
            }
        }
    }

    /* Validate effects */
    for (size_t i = 0; i < contract->effects_count; i++) {
        const bs_contract_effect_t* e = &contract->effects[i];

        if (e->trigger[0] == '\0') {
            APPEND_ERROR("effect[%zu]: trigger must be non-empty", i);
        }
    }

#undef APPEND_ERROR

    return errors;
}
