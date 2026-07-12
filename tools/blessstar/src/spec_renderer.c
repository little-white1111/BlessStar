#include "blessstar/spec_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Internal structure for the template context                        */
/* ------------------------------------------------------------------ */

/* Forward declarations */
typedef struct context context_t;

/* Callback to lookup a variable by path in the contract */
typedef const char* (*var_lookup_fn)(const context_t* ctx, const char* path);

struct context {
    const bs_contract_t* contract;
    const char* lang;
    var_lookup_fn lookup;
};

/* ------------------------------------------------------------------ */
/*  Variable lookup helpers                                            */
/* ------------------------------------------------------------------ */

/**
 * Lookup a top-level contract field by name.
 */
static const char* lookup_top_level(const bs_contract_t* c, const char* name) {
    if (strcmp(name, "biz_id") == 0)           return c->biz_id;
    if (strcmp(name, "display_name") == 0)     return c->display_name;
    if (strcmp(name, "version") == 0)          return c->version;
    if (strcmp(name, "sdk_version") == 0)      return c->sdk_version;
    if (strcmp(name, "description") == 0)      return c->description;
    if (strcmp(name, "fields_count") == 0)     return NULL; /* numeric, not string */
    if (strcmp(name, "gates_count") == 0)      return NULL;
    if (strcmp(name, "effects_count") == 0)    return NULL;
    if (strcmp(name, "dependencies_count") == 0) return NULL;
    if (strcmp(name, "project_root") == 0)     return ".";
    if (strcmp(name, "module_dir") == 0)       return "adapter/business";
    if (strcmp(name, "cmake_target") == 0)     return "bs_app_sdk";
    if (strcmp(name, "test_filter") == 0)      return c->biz_id;
    return NULL;
}

/**
 * Lookup a field property within a specific field at given index.
 */
static const char* lookup_field_prop(const bs_contract_field_t* f, const char* prop) {
    if (strcmp(prop, "key") == 0)          return f->key;
    if (strcmp(prop, "type") == 0)         return bs_contract_field_type_to_upper(f->type);
    if (strcmp(prop, "type_upper") == 0)   return bs_contract_field_type_to_upper(f->type);
    if (strcmp(prop, "default") == 0)      return f->default_str;
    if (strcmp(prop, "description") == 0)  return f->description;
    if (strcmp(prop, "required") == 0)     return f->required ? "true" : "false";
    return NULL;
}

/**
 * Lookup a gate property within a specific gate at given index.
 */
static const char* lookup_gate_prop(const bs_contract_gate_t* g, const char* prop) {
    if (strcmp(prop, "id") == 0)            return g->id;
    if (strcmp(prop, "description") == 0)   return g->description;
    if (strcmp(prop, "field") == 0)         return g->field;
    if (strcmp(prop, "rule") == 0)          return g->rule;
    if (strcmp(prop, "layer") == 0)         return g->layer;
    if (strcmp(prop, "sub_category") == 0)  return g->sub_category;
    if (strcmp(prop, "scenario") == 0)      return g->scenario;
    return NULL;
}

/**
 * Lookup an effect property within a specific effect at given index.
 */
static const char* lookup_effect_prop(const bs_contract_effect_t* e, const char* prop) {
    if (strcmp(prop, "trigger") == 0)                    return e->trigger;
    if (strcmp(prop, "action") == 0)                     return e->action;
    if (strcmp(prop, "async") == 0)                      return e->async ? "true" : "false";
    if (strcmp(prop, "trigger_dot_to_underscore") == 0) {
        static char buf[BS_CONTRACT_MAX_KEY_LEN];
        size_t i;
        for (i = 0; i < sizeof(buf) - 1 && e->trigger[i]; i++) {
            buf[i] = (e->trigger[i] == '.') ? '_' : e->trigger[i];
        }
        buf[i] = '\0';
        return buf;
    }
    return NULL;
}

/**
 * Lookup a dependency value at given index.
 */
static const char* lookup_dep_value(const bs_contract_t* c, size_t idx) {
    if (idx < c->dependencies_count) {
        return c->dependencies[idx];
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Dot-separated path resolution                                      */
/* ------------------------------------------------------------------ */

/**
 * Resolve a dot-separated path like "fields.[0].key" or "biz_id".
 * Returns the string value or NULL if not found.
 */
static const char* resolve_path(const context_t* ctx, const char* path) {
    if (!ctx || !ctx->contract || !path) return NULL;

    const bs_contract_t* c = ctx->contract;

    /* Try top-level first */
    const char* val = lookup_top_level(c, path);
    if (val) return val;

    /* Handle lang */
    if (strcmp(path, "lang") == 0) {
        return ctx->lang ? ctx->lang : "c";
    }

    /* Handle biz_id_underscore transform */
    if (strcmp(path, "biz_id_underscore") == 0) {
        static char buf[BS_CONTRACT_MAX_KEY_LEN];
        size_t i;
        for (i = 0; i < sizeof(buf) - 1 && c->biz_id[i]; i++) {
            buf[i] = (c->biz_id[i] == '.') ? '_' : c->biz_id[i];
        }
        buf[i] = '\0';
        return buf;
    }

    /* Handle fields.[N].prop */
    if (strncmp(path, "fields.", 7) == 0) {
        const char* rest = path + 7;
        if (*rest == '[') {
            rest++;
            char* end = NULL;
            long idx = strtol(rest, &end, 10);
            if (end && *end == ']' && *(end + 1) == '.' && idx >= 0 && (size_t)idx < c->fields_count) {
                const char* prop = end + 2;
                return lookup_field_prop(&c->fields[idx], prop);
            }
        }
    }

    /* Handle gates.[N].prop */
    if (strncmp(path, "gates.", 6) == 0) {
        const char* rest = path + 6;
        if (*rest == '[') {
            rest++;
            char* end = NULL;
            long idx = strtol(rest, &end, 10);
            if (end && *end == ']' && *(end + 1) == '.' && idx >= 0 && (size_t)idx < c->gates_count) {
                const char* prop = end + 2;
                return lookup_gate_prop(&c->gates[idx], prop);
            }
        }
    }

    /* Handle effects.[N].prop */
    if (strncmp(path, "effects.", 8) == 0) {
        const char* rest = path + 8;
        if (*rest == '[') {
            rest++;
            char* end = NULL;
            long idx = strtol(rest, &end, 10);
            if (end && *end == ']' && *(end + 1) == '.' && idx >= 0 && (size_t)idx < c->effects_count) {
                const char* prop = end + 2;
                return lookup_effect_prop(&c->effects[idx], prop);
            }
        }
    }

    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Mustache template rendering engine                                 */
/* ------------------------------------------------------------------ */

/**
 * Skip whitespace in a string.
 */
static const char* skip_ws(const char* p) {
    while (*p && (*p == ' ' || *p == '\t')) p++;
    return p;
}

/**
 * Check if a section name corresponds to a list/array in the contract.
 */
static int is_list_section(const bs_contract_t* c, const char* name) {
    if (strcmp(name, "fields") == 0)       return c->fields_count > 0;
    if (strcmp(name, "gates") == 0)        return c->gates_count > 0;
    if (strcmp(name, "effects") == 0)      return c->effects_count > 0;
    if (strcmp(name, "dependencies") == 0) return c->dependencies_count > 0;
    return 0;
}

/**
 * Check if a section is truthy (exists and has value).
 * For lists, returns non-zero if the list has items.
 * For variables, returns non-zero if the variable exists and is non-empty.
 */
static int is_section_truthy(const bs_contract_t* c, const char* name) {
    /* Check top-level fields */
    const char* val = lookup_top_level(c, name);
    if (val) return (val[0] != '\0');

    /* Check if it's a list */
    if (is_list_section(c, name)) return 1;

    return 0;
}

/**
 * Check if a section is falsy (inverted).
 */
static int is_section_falsy(const bs_contract_t* c, const char* name) {
    return !is_section_truthy(c, name);
}

/**
 * Get the count of items in a named list section.
 */
static size_t get_list_count(const bs_contract_t* c, const char* name) {
    if (strcmp(name, "fields") == 0)       return c->fields_count;
    if (strcmp(name, "gates") == 0)        return c->gates_count;
    if (strcmp(name, "effects") == 0)      return c->effects_count;
    if (strcmp(name, "dependencies") == 0) return c->dependencies_count;
    return 0;
}

/**
 * Render a single variable tag {{...}}.
 * Writes resolved value to output.
 */
static void render_tag(const context_t* ctx, const char* tag_content, FILE* output) {
    const char* val = resolve_path(ctx, tag_content);
    if (val) {
        fputs(val, output);
    }
}

/**
 * Recursively render a template string.
 * Returns pointer to the character after the processed section,
 * or NULL on error.
 */
static const char* render_template_internal(const context_t* ctx, const char* template_text, FILE* output) {
    const char* p = template_text;

    while (*p) {
        /* Find next tag start */
        const char* tag_start = strstr(p, "{{");
        if (!tag_start) {
            /* No more tags, write rest as literal */
            fputs(p, output);
            return p + strlen(p);
        }

        /* Write everything before the tag */
        fwrite(p, 1, (size_t)(tag_start - p), output);

        /* Find tag end */
        const char* tag_end = strstr(tag_start + 2, "}}");
        if (!tag_end) {
            /* Unclosed tag, write rest as literal */
            fputs(tag_start, output);
            return p + strlen(p);
        }

        /* Extract tag content (between {{ and }}) */
        size_t content_len = (size_t)(tag_end - tag_start - 2);
        char* tag_content = (char*)malloc(content_len + 1);
        if (!tag_content) return NULL;
        strncpy(tag_content, tag_start + 2, content_len);
        tag_content[content_len] = '\0';

        /* Trim whitespace from tag content */
        const char* trimmed = skip_ws(tag_content);
        
        /* Determine tag type */
        if (trimmed[0] == '#') {
            /* Section tag: {{#section}}...{{/section}} */
            const char* section_name = skip_ws(trimmed + 1);
            
            /* Find matching closing tag */
            char close_tag_start[256];
            char close_tag_full[256];
            snprintf(close_tag_start, sizeof(close_tag_start), "{{/%s", section_name);
            snprintf(close_tag_full, sizeof(close_tag_full), "{{/%s}}", section_name);
            
            const char* section_end = strstr(tag_end + 2, close_tag_full);
            if (!section_end) {
                /* Try finding just the start and closing brace */
                const char* search = tag_end + 2;
                while (*search) {
                    if (strncmp(search, close_tag_start, strlen(close_tag_start)) == 0) {
                        const char* brace = strstr(search, "}}");
                        if (brace) {
                            section_end = brace;
                            break;
                        }
                    }
                    search++;
                }
                if (!section_end) {
                    /* No matching closing tag, write tag as literal */
                    fwrite(tag_start, 1, (size_t)(tag_end - tag_start + 2), output);
                    free(tag_content);
                    p = tag_end + 2;
                    continue;
                }
            }

            size_t section_len = (size_t)(section_end - tag_end - 2);
            char* section_content = (char*)malloc(section_len + 1);
            if (!section_content) {
                free(tag_content);
                return NULL;
            }
            strncpy(section_content, tag_end + 2, section_len);
            section_content[section_len] = '\0';

            /* Check if this is a list section */
            size_t list_count = get_list_count(ctx->contract, section_name);
            
            if (list_count > 0) {
                /* Iterate over list items */
                for (size_t i = 0; i < list_count; i++) {
                    /* Create a sub-context pointing to current index for path resolution */
                    /* We handle iteration by rendering the section content with index-aware paths */
                    const bs_contract_t* c = ctx->contract;
                    
                    /* Render the section content, replacing .[0]. with .[i]. */
                    /* Simplification: re-parse the section content recursively */
                    const char* sp = section_content;
                    while (*sp) {
                        const char* st = strstr(sp, "{{");
                        if (!st) {
                            fputs(sp, output);
                            break;
                        }
                        fwrite(sp, 1, (size_t)(st - sp), output);
                        const char* se = strstr(st + 2, "}}");
                        if (!se) {
                            fputs(st, output);
                            break;
                        }
                        size_t cl = (size_t)(se - st - 2);
                        char* tc = (char*)malloc(cl + 1);
                        if (!tc) break;
                        strncpy(tc, st + 2, cl);
                        tc[cl] = '\0';
                        
                        /* Replace [0] with [i] in the tag for list iteration */
                        char resolved_tag[512];
                        const char* br_start = strstr(tc, "[0]");
                        if (br_start) {
                            size_t prefix_len = (size_t)(br_start - tc);
                            size_t br_end_pos = prefix_len + 3; /* skip [0] */
                            snprintf(resolved_tag, sizeof(resolved_tag), 
                                     "%.*s[%zu]%s",
                                     (int)prefix_len, tc, i,
                                     tc + br_end_pos);
                        } else {
                            snprintf(resolved_tag, sizeof(resolved_tag), "%s", tc);
                        }
                        
                        const char* val = resolve_path(ctx, resolved_tag);
                        if (val) fputs(val, output);
                        
                        free(tc);
                        sp = se + 2;
                    }
                }
            } else if (is_section_truthy(ctx->contract, section_name)) {
                /* Truthy section: render content once */
                render_template_internal(ctx, section_content, output);
            }
            /* If falsy, skip content */

            free(section_content);
            p = section_end + strlen(close_tag_full);
            
        } else if (trimmed[0] == '^') {
            /* Inverted section: {{^section}}...{{/section}} */
            const char* section_name = skip_ws(trimmed + 1);
            
            char close_tag_full[256];
            snprintf(close_tag_full, sizeof(close_tag_full), "{{/%s}}", section_name);
            
            const char* section_end = strstr(tag_end + 2, close_tag_full);
            if (!section_end) {
                fwrite(tag_start, 1, (size_t)(tag_end - tag_start + 2), output);
                free(tag_content);
                p = tag_end + 2;
                continue;
            }

            if (is_section_falsy(ctx->contract, section_name)) {
                size_t section_len = (size_t)(section_end - tag_end - 2);
                char* section_content = (char*)malloc(section_len + 1);
                if (!section_content) {
                    free(tag_content);
                    return NULL;
                }
                strncpy(section_content, tag_end + 2, section_len);
                section_content[section_len] = '\0';
                render_template_internal(ctx, section_content, output);
                free(section_content);
            }

            p = section_end + strlen(close_tag_full);
            
        } else {
            /* Simple variable tag: {{var}} */
            render_tag(ctx, trimmed, output);
            p = tag_end + 2;
        }

        free(tag_content);
    }

    return p;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

int bs_render_template(const bs_contract_t* contract, const char* template_text,
                        const char* lang, FILE* output) {
    if (!contract || !template_text || !output) return -1;

    context_t ctx;
    ctx.contract = contract;
    ctx.lang = lang ? lang : "c";
    ctx.lookup = resolve_path;

    if (!render_template_internal(&ctx, template_text, output)) {
        return -1;
    }

    return 0;
}

int bs_render_template_file(const bs_contract_t* contract, const char* template_path,
                             const char* lang, FILE* output) {
    if (!contract || !template_path || !output) return -1;

    FILE* fp = fopen(template_path, "rb");
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

    int result = bs_render_template(contract, buffer, lang, output);
    free(buffer);
    return result;
}
