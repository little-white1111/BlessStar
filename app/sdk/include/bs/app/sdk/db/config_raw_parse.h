#ifndef BS_APP_SDK_DB_CONFIG_RAW_PARSE_H
#define BS_APP_SDK_DB_CONFIG_RAW_PARSE_H

/*
 * C ABI for converting raw vendor formats (INI/XML/YAML) to BlessStar v1 JSON.
 * Used by NormalizeVendorConfig when VendorFormat is RawIni/RawXml/RawYaml.
 * Implemented in adapter/parser for now; linked into bs_app_sdk.
 */

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse raw INI bytes into v1 JSON bytes.
 * Returns 0 on success, negative on error. Caller frees *out via free().
 */
int bs_config_raw_ini_to_json(const uint8_t* data, size_t len,
                              uint8_t** out, size_t* out_len);

/**
 * Parse raw XML bytes into v1 JSON bytes.
 * Returns 0 on success, negative on error. Caller frees *out via free().
 */
int bs_config_raw_xml_to_json(const uint8_t* data, size_t len,
                              uint8_t** out, size_t* out_len);

/**
 * Parse raw YAML bytes into v1 JSON bytes.
 * Returns 0 on success, negative on error. Caller frees *out via free().
 */
int bs_config_raw_yaml_to_json(const uint8_t* data, size_t len,
                               uint8_t** out, size_t* out_len);

#ifdef __cplusplus
}
#endif

#endif // BS_APP_SDK_DB_CONFIG_RAW_PARSE_H
