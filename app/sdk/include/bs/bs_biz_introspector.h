#ifndef BS_BIZ_INTROSPECTOR_H
#define BS_BIZ_INTROSPECTOR_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Business symbol categories ────────────────────────────────────── */
typedef enum {
    BS_BIZ_SYMBOL_CLASS     = 0,
    BS_BIZ_SYMBOL_FUNCTION  = 1,
    BS_BIZ_SYMBOL_INTERFACE = 2,
    BS_BIZ_SYMBOL_STRUCT    = 3,
    BS_BIZ_SYMBOL_ENUM      = 4
} bs_biz_symbol_category_t;

/* ── Introspect configuration ──────────────────────────────────────── */
typedef struct bs_biz_introspect_config {
    const char*  src_dir;
    const char*  output_dir;
    const char*  language;
    const char** scope_patterns;
    size_t       scope_count;
    bool         use_ai_enhance;
} bs_biz_introspect_config_t;

/* ── Run introspection and produce biz_semantic_index.json ─────────── */
int  bs_biz_introspect(const bs_biz_introspect_config_t* config,
                        char** out_index_json, size_t* out_len);
void bs_biz_introspect_free(char* json);

#ifdef __cplusplus
}
#endif

#endif /* BS_BIZ_INTROSPECTOR_H */
