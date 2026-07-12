#ifndef BS_ADAPTER_PARSER_SCHEMA_IMPORT_H
#define BS_ADAPTER_PARSER_SCHEMA_IMPORT_H

/*
 * C-ST-7 contract block:
 * Thread safety: reentrant, no global state.
 * Error semantics: negative errno codes on error.
 * Platform notes: N/A.
 *
 * ADR-全链路接通 — External JSON Schema ↔ internal schema conversion.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Import an external JSON Schema (Draft-07) and convert to
     * BlessStar-compatible schema metadata JSON.
     *
     * @param json_schema     Input JSON Schema bytes.
     * @param schema_len      Input length.
     * @param out             Output BlessStar schema JSON bytes (caller free()).
     * @param out_len         Output length.
     * @return 0 on success, negative errno on error.
     */
    int bs_schema_import(const uint8_t* json_schema, size_t schema_len,
                         uint8_t** out, size_t* out_len);

    /**
     * Export a BlessStar internal schema as JSON Schema Draft-07.
     *
     * @param bs_schema_json  BlessStar schema JSON bytes.
     * @param schema_len      Input length.
     * @param out             Output JSON Schema bytes (caller free()).
     * @param out_len         Output length.
     * @return 0 on success, negative errno on error.
     */
    int bs_schema_export(const uint8_t* bs_schema_json, size_t schema_len,
                         uint8_t** out, size_t* out_len);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PARSER_SCHEMA_IMPORT_H */
