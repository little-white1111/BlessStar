#ifndef BS_ADAPTER_PARSER_CONFIG_FORMAT_CONVERT_H
#define BS_ADAPTER_PARSER_CONFIG_FORMAT_CONVERT_H

/*
 * C-ST-7 contract block:
 * Thread safety: all functions are reentrant; no global state.
 * Error semantics: negative errno codes on error, 0 on success.
 *                Caller must free() output buffers on success.
 * Platform notes: N/A.
 *
 * ADR-全链路接通 — 架构不变量 #5 (格式转换双向幂等):
 *   v1_json ↔ yaml ↔ toml ↔ ini 转换必须是 round-trip 安全的，
 *   任意转换两次回到原格式后语义等价。
 */

#include "format_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Convert v1 JSON bytes to the target serialization format.
     *
     * @param v1_json     Input v1 JSON bytes (null-terminated).
     * @param v1_len      Length of input including null terminator.
     * @param target_fmt  Target output format.
     * @param out         On success, caller-owned output bytes (free() to release).
     * @param out_len     Output length including null terminator.
     * @return 0 on success, negative errno on error.
     */
    int bs_format_convert(const uint8_t* v1_json, size_t v1_len,
                          bs_format_t target_fmt,
                          uint8_t** out, size_t* out_len);

    /**
     * Auto-detect input format from content bytes and/or filename hint.
     *
     * Detection priority:
     *   1. Content magic bytes / header patterns
     *   2. File extension (if filename_hint is provided)
     *   3. Falls back to BS_FORMAT_JSON
     *
     * @param data          Input bytes (at least 8 bytes recommended).
     * @param len           Length of input data.
     * @param filename_hint Optional filename or extension (e.g. "config.yaml").
     *                      May be NULL.
     * @return Detected format with confidence score.
     */
    bs_format_detect_result_t bs_format_detect(const uint8_t* data, size_t len,
                                                const char* filename_hint);

    /**
     * Convert a flat config entries array to v1 JSON bytes.
     * Used internally by workspace when reading external formats.
     */
    int bs_entries_to_v1_json(const bs_config_entry_t* entries, size_t count,
                              uint8_t** out, size_t* out_len);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PARSER_CONFIG_FORMAT_CONVERT_H */
