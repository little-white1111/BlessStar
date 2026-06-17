#ifndef BS_KERNEL_SCHEMA_JSON_CONVERTER_H
#define BS_KERNEL_SCHEMA_JSON_CONVERTER_H

/*
 * Bidirectional converter between BlessStar Compact Schema and JSON Schema Draft-07.
 *
 * C-ST-7 contract block:
 * Thread safety: reentrant.
 * Error semantics: int return; negative on error.
 * Platform notes: Pure C; produces JSON string output (caller free()).
 */

#include "schema_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Convert a BlessStar Compact Schema entry to JSON Schema Draft-07 string.
 * The output JSON is allocated via malloc(); caller must free().
 *
 * BlessStar-specific fields (ai_hint, ui.*, custom_validator) are placed
 * into the "x-blessstar" extension slot for round-trip preservation.
 */
    int bs_json_converter_to_draft07(const bs_schema_entry_t* entry,
                                      char** out_json, size_t* out_len);

/**
 * Convert a JSON Schema Draft-07 string to a BlessStar Compact Schema entry.
 * The returned entry is heap-allocated; caller must free with
 * free_entry_fields and then free() the entry pointer.
 *
 * If ai_hint is missing for any field, a WARNING is emitted and
 * an empty ai_hint placeholder is generated.
 * Fields outside "x-blessstar" and known JSON Schema keywords are ignored.
 */
    int bs_json_converter_from_draft07(const char* json_string,
                                        size_t json_len,
                                        bs_schema_entry_t** out_entry);

/**
 * Free the contents (but not the pointer itself) of a schema_entry
 * allocated by bs_json_converter_from_draft07.
 */
    void bs_json_converter_free_entry(bs_schema_entry_t* entry);

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_SCHEMA_JSON_CONVERTER_H */
