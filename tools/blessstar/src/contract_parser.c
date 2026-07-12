#include "blessstar/contract_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Internal section tracking enum for the state machine */
typedef enum {
    SEC_NONE, SEC_FIELDS, SEC_DEPS, SEC_GATES, SEC_EFFECTS,
    SEC_GATE_ITEM, SEC_EFFECT_ITEM, SEC_FIELD_ITEM
} section_t;

/* Forward declarations of helper functions */
static void trim_leading_spaces(const char* line, char* out, size_t out_size);
static int count_indent(const char* line);
static int parse_key_value(const char* line, char* key, size_t key_size, char* value, size_t value_size);
static int parse_top_level_field(const char* key, const char* value, bs_contract_t* out);
static int parse_field_property(const char* key, const char* value, bs_contract_field_t* field);
static int parse_gate_property(const char* key, const char* value, bs_contract_gate_t* gate);
static int parse_effect_property(const char* key, const char* value, bs_contract_effect_t* effect);

/*
 * Helper: trim leading whitespace from a line.
 */
static void trim_leading_spaces(const char* line, char* out, size_t out_size) {
    if (!line || !out || out_size == 0) return;
    while (*line && isspace((unsigned char)*line)) line++;
    snprintf(out, out_size, "%s", line);
}

/*
 * Helper: count leading spaces for indentation.
 */
static int count_indent(const char* line) {
    int n = 0;
    while (line[n] == ' ') n++;
    return n;
}

/*
 * Helper: parse a "key: value" line. Returns 1 on success, 0 on failure.
 * Strips quotes from quoted values.
 */
static int parse_key_value(const char* line, char* key, size_t key_size, char* value, size_t value_size) {
    const char* colon = strchr(line, ':');
    if (!colon) return 0;

    size_t key_len = colon - line;
    if (key_len >= key_size) key_len = key_size - 1;
    strncpy(key, line, key_len);
    key[key_len] = '\0';

    /* Trim trailing spaces from key */
    while (key_len > 0 && isspace((unsigned char)key[key_len - 1])) key_len--;
    key[key_len] = '\0';

    const char* v = colon + 1;
    while (*v && isspace((unsigned char)*v)) v++;

    /* Handle quoted values */
    if (*v == '"' || *v == '\'') {
        char quote = *v;
        v++;
        const char* end = strchr(v, quote);
        if (end) {
            size_t val_len = end - v;
            if (val_len >= value_size) val_len = value_size - 1;
            strncpy(value, v, val_len);
            value[val_len] = '\0';
        } else {
            snprintf(value, value_size, "%s", v);
        }
    } else {
        snprintf(value, value_size, "%s", v);
    }

    return 1;
}

/*
 * Helper: determine if a line is a section header like "fields:", "gates:", etc.
 */
static int is_section_header(const char* key) {
    return (strcmp(key, "fields") == 0 || strcmp(key, "dependencies") == 0 ||
            strcmp(key, "gates") == 0 || strcmp(key, "effects") == 0);
}

/*
 * Map top-level key-value pairs to contract struct fields.
 */
static int parse_top_level_field(const char* key, const char* value, bs_contract_t* out) {
    if (strcmp(key, "biz_id") == 0) {
        snprintf(out->biz_id, sizeof(out->biz_id), "%s", value);
    } else if (strcmp(key, "display_name") == 0) {
        snprintf(out->display_name, sizeof(out->display_name), "%s", value);
    } else if (strcmp(key, "version") == 0) {
        snprintf(out->version, sizeof(out->version), "%s", value);
    } else if (strcmp(key, "sdk_version") == 0) {
        snprintf(out->sdk_version, sizeof(out->sdk_version), "%s", value);
    } else if (strcmp(key, "description") == 0) {
        snprintf(out->description, sizeof(out->description), "%s", value);
    } else {
        /* Unknown top-level key - skip silently */
        return 0;
    }
    return 1;
}

/*
 * Map field sub-properties (inside a fields list item).
 */
static int parse_field_property(const char* key, const char* value, bs_contract_field_t* field) {
    if (strcmp(key, "key") == 0) {
        snprintf(field->key, sizeof(field->key), "%s", value);
    } else if (strcmp(key, "type") == 0) {
        field->type = bs_contract_field_type_from_str(value);
    } else if (strcmp(key, "default") == 0) {
        snprintf(field->default_str, sizeof(field->default_str), "%s", value);
    } else if (strcmp(key, "description") == 0) {
        snprintf(field->description, sizeof(field->description), "%s", value);
    } else if (strcmp(key, "required") == 0) {
        field->required = (strcmp(value, "true") == 0 || strcmp(value, "yes") == 0 || strcmp(value, "1") == 0);
    } else {
        return 0;
    }
    return 1;
}

/*
 * Map gate sub-properties (inside a gates list item).
 */
static int parse_gate_property(const char* key, const char* value, bs_contract_gate_t* gate) {
    if (strcmp(key, "id") == 0) {
        snprintf(gate->id, sizeof(gate->id), "%s", value);
    } else if (strcmp(key, "description") == 0) {
        snprintf(gate->description, sizeof(gate->description), "%s", value);
    } else if (strcmp(key, "field") == 0) {
        snprintf(gate->field, sizeof(gate->field), "%s", value);
    } else if (strcmp(key, "rule") == 0) {
        snprintf(gate->rule, sizeof(gate->rule), "%s", value);
    } else if (strcmp(key, "layer") == 0) {
        snprintf(gate->layer, sizeof(gate->layer), "%s", value);
    } else if (strcmp(key, "sub_category") == 0) {
        snprintf(gate->sub_category, sizeof(gate->sub_category), "%s", value);
    } else if (strcmp(key, "scenario") == 0) {
        snprintf(gate->scenario, sizeof(gate->scenario), "%s", value);
    } else if (strcmp(key, "dependencies") == 0) {
        /* Dependencies inside a gate: comma-separated list */
        const char* p = value;
        while (*p && gate->dependencies_count < BS_CONTRACT_MAX_DEPS) {
            while (*p && isspace((unsigned char)*p)) p++;
            if (!*p) break;
            const char* start = p;
            while (*p && *p != ',') p++;
            size_t len = p - start;
            if (len >= BS_CONTRACT_MAX_KEY_LEN) len = BS_CONTRACT_MAX_KEY_LEN - 1;
            strncpy(gate->dependencies[gate->dependencies_count], start, len);
            gate->dependencies[gate->dependencies_count][len] = '\0';
            gate->dependencies_count++;
            if (*p == ',') p++;
        }
    } else {
        return 0;
    }
    return 1;
}

/*
 * Map effect sub-properties (inside an effects list item).
 */
static int parse_effect_property(const char* key, const char* value, bs_contract_effect_t* effect) {
    if (strcmp(key, "trigger") == 0) {
        snprintf(effect->trigger, sizeof(effect->trigger), "%s", value);
    } else if (strcmp(key, "action") == 0) {
        snprintf(effect->action, sizeof(effect->action), "%s", value);
    } else if (strcmp(key, "async") == 0) {
        effect->async = (strcmp(value, "true") == 0 || strcmp(value, "yes") == 0 || strcmp(value, "1") == 0);
    } else {
        return 0;
    }
    return 1;
}

int bs_contract_parse(const char* yaml_text, bs_contract_t* out) {
    if (!yaml_text || !out) return -1;

    memset(out, 0, sizeof(bs_contract_t));

    const char* line_start = yaml_text;
    section_t current_section = SEC_NONE;
    section_t parent_section = SEC_NONE;
    int in_list_item = 0;
    int last_indent = 0;
    int gate_list_active = 0;
    int effect_list_active = 0;
    int field_list_active = 0;
    int dep_list_active = 0;

    while (*line_start) {
        /* Find end of line */
        const char* line_end = line_start;
        while (*line_end && *line_end != '\n') line_end++;

        /* Build a null-terminated copy of the line */
        size_t line_len = line_end - line_start;
        char* line = (char*)malloc(line_len + 1);
        if (!line) return -1;
        strncpy(line, line_start, line_len);
        line[line_len] = '\0';

        /* Skip empty lines and comment lines */
        const char* trimmed = line;
        while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;
        if (*trimmed == '\0' || *trimmed == '#') {
            free(line);
            line_start = *line_end ? line_end + 1 : line_end;
            continue;
        }

        int indent = count_indent(line);
        char key[BS_CONTRACT_MAX_KEY_LEN];
        char value[BS_CONTRACT_MAX_STR_LEN];

        /* Check if this is a list item (starts with - after trimming) */
        char trimmed_line[1024];
        trim_leading_spaces(line, trimmed_line, sizeof(trimmed_line));

        if (trimmed_line[0] == '-') {
            /* List item */
            const char* item_content = trimmed_line + 1;
            while (*item_content && isspace((unsigned char)*item_content)) item_content++;

            if (parent_section == SEC_FIELDS) {
                if (out->fields_count < BS_CONTRACT_MAX_FIELDS) {
                    memset(&out->fields[out->fields_count], 0, sizeof(bs_contract_field_t));
                    out->fields_count++;
                }
                current_section = SEC_FIELD_ITEM;
                field_list_active = 1;
            } else if (parent_section == SEC_GATES) {
                if (out->gates_count < BS_CONTRACT_MAX_GATES) {
                    memset(&out->gates[out->gates_count], 0, sizeof(bs_contract_gate_t));
                    out->gates_count++;
                }
                current_section = SEC_GATE_ITEM;
                gate_list_active = 1;
            } else if (parent_section == SEC_EFFECTS) {
                if (out->effects_count < BS_CONTRACT_MAX_EFFECTS) {
                    memset(&out->effects[out->effects_count], 0, sizeof(bs_contract_effect_t));
                    out->effects_count++;
                }
                current_section = SEC_EFFECT_ITEM;
                effect_list_active = 1;
            } else if (parent_section == SEC_DEPS) {
                /* Dependency list items: the item content is the dependency name */
                if (out->dependencies_count < BS_CONTRACT_MAX_DEPS) {
                    snprintf(out->dependencies[out->dependencies_count],
                             BS_CONTRACT_MAX_KEY_LEN, "%s", item_content);
                    out->dependencies_count++;
                }
                dep_list_active = 1;
            }

            /* Check if item_content has key:value (inline after -) */
            if (parse_key_value(item_content, key, sizeof(key), value, sizeof(value))) {
                /* Process inline key:value */
                if (current_section == SEC_FIELD_ITEM && out->fields_count > 0) {
                    parse_field_property(key, value, &out->fields[out->fields_count - 1]);
                } else if (current_section == SEC_GATE_ITEM && out->gates_count > 0) {
                    parse_gate_property(key, value, &out->gates[out->gates_count - 1]);
                } else if (current_section == SEC_EFFECT_ITEM && out->effects_count > 0) {
                    parse_effect_property(key, value, &out->effects[out->effects_count - 1]);
                }
            }

            in_list_item = 1;
        } else if (parse_key_value(trimmed_line, key, sizeof(key), value, sizeof(value))) {
            /* Key-value pair */
            in_list_item = 0;

            if (indent == 0 && !is_section_header(key)) {
                /* Top-level key-value */
                parse_top_level_field(key, value, out);
                current_section = SEC_NONE;
                parent_section = SEC_NONE;
            } else if (indent == 0 && is_section_header(key)) {
                /* Section header at top level */
                if (strcmp(key, "fields") == 0) {
                    parent_section = SEC_FIELDS;
                } else if (strcmp(key, "dependencies") == 0) {
                    parent_section = SEC_DEPS;
                } else if (strcmp(key, "gates") == 0) {
                    parent_section = SEC_GATES;
                } else if (strcmp(key, "effects") == 0) {
                    parent_section = SEC_EFFECTS;
                }
                current_section = SEC_NONE;
            } else if (indent > 0) {
                /* Nested key-value: belongs to current list item */
                if (current_section == SEC_FIELD_ITEM && out->fields_count > 0) {
                    parse_field_property(key, value, &out->fields[out->fields_count - 1]);
                } else if (current_section == SEC_GATE_ITEM && out->gates_count > 0) {
                    parse_gate_property(key, value, &out->gates[out->gates_count - 1]);
                } else if (current_section == SEC_EFFECT_ITEM && out->effects_count > 0) {
                    parse_effect_property(key, value, &out->effects[out->effects_count - 1]);
                }
            }
        }

        last_indent = indent;
        free(line);
        line_start = *line_end ? line_end + 1 : line_end;
    }

    return 0;
}

int bs_contract_parse_file(const char* file_path, bs_contract_t* out) {
    if (!file_path || !out) return -1;

    FILE* fp = fopen(file_path, "rb");
    if (!fp) return -1;

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    if (fsize < 0) {
        fclose(fp);
        return -1;
    }
    fseek(fp, 0, SEEK_SET);

    char* buffer = (char*)malloc((size_t)fsize + 1);
    if (!buffer) {
        fclose(fp);
        return -1;
    }

    size_t read_size = fread(buffer, 1, (size_t)fsize, fp);
    buffer[read_size] = '\0';
    fclose(fp);

    int result = bs_contract_parse(buffer, out);
    free(buffer);
    return result;
}

void bs_contract_destroy(bs_contract_t* contract) {
    (void)contract;
    /* Currently no dynamically allocated memory to free */
}

bs_contract_field_type_t bs_contract_field_type_from_str(const char* type_str) {
    if (!type_str) return BS_FIELD_TYPE_STRING;

    char lower[32];
    size_t i;
    for (i = 0; i < sizeof(lower) - 1 && type_str[i]; i++) {
        lower[i] = (char)tolower((unsigned char)type_str[i]);
    }
    lower[i] = '\0';

    if (strcmp(lower, "int32") == 0) return BS_FIELD_TYPE_INT32;
    if (strcmp(lower, "int64") == 0) return BS_FIELD_TYPE_INT64;
    if (strcmp(lower, "string") == 0) return BS_FIELD_TYPE_STRING;
    if (strcmp(lower, "double") == 0) return BS_FIELD_TYPE_DOUBLE;
    if (strcmp(lower, "bool") == 0) return BS_FIELD_TYPE_BOOL;
    if (strcmp(lower, "file") == 0) return BS_FIELD_TYPE_FILE;

    return BS_FIELD_TYPE_STRING;
}

const char* bs_contract_field_type_to_upper(bs_contract_field_type_t t) {
    switch (t) {
        case BS_FIELD_TYPE_INT32:  return "INT32";
        case BS_FIELD_TYPE_INT64:  return "INT64";
        case BS_FIELD_TYPE_STRING: return "STRING";
        case BS_FIELD_TYPE_DOUBLE: return "DOUBLE";
        case BS_FIELD_TYPE_BOOL:   return "BOOL";
        case BS_FIELD_TYPE_FILE:   return "FILE";
        default:                   return "STRING";
    }
}
