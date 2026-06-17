#ifndef BS_KERNEL_UI_MAP_SCHEMA_TO_UIDL_H
#define BS_KERNEL_UI_MAP_SCHEMA_TO_UIDL_H

/*
 * Schema -> UIDL JSON converter.
 *
 * C-ST-7 contract block:
 * Thread safety: reentrant.
 * Error semantics: int return; negative on error.
 * Platform notes: Pure C; produces JSON string output (caller free()).
 */

#include <stddef.h>
#include <bs/kernel/schema/schema_types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Convert a BlessStar Compact Schema entry to UIDL JSON string.
 * The output JSON is allocated via malloc(); caller must free().
 *
 * Mapping rules (per user-confirmed decision):
 * | Schema type     | UIDL widget default | Overridable to              |
 * |-----------------|---------------------|-----------------------------|
 * | string          | input               | textarea, select, datepicker|
 * | string + enum   | select              | radio, checkbox             |
 * | i32 / i64       | number              | input                       |
 * | f64             | number              | —                           |
 * | bool            | checkbox            | switch, radio               |
 * | object          | group               | table                       |
 * | array           | repeatable_group    | table                       |
 *
 * ai_layout_hint is passed through directly from the schema ai_hint field.
 */
    int bs_schema_to_uidl(const bs_schema_entry_t* entry,
                           char** out_json, size_t* out_len);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_UI_MAP_SCHEMA_TO_UIDL_H */
