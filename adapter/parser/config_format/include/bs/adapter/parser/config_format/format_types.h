#ifndef BS_ADAPTER_PARSER_CONFIG_FORMAT_TYPES_H
#define BS_ADAPTER_PARSER_CONFIG_FORMAT_TYPES_H

/*
 * C-ST-7 contract block:
 * Thread safety: all functions are reentrant; no global state.
 * Error semantics: negative errno codes on error, 0 on success.
 * Platform notes: N/A.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /** Supported config serialization formats. */
    typedef enum bs_format_t
    {
        BS_FORMAT_JSON = 0, /**< RFC 8259 JSON */
        BS_FORMAT_YAML,     /**< YAML 1.2 */
        BS_FORMAT_TOML,     /**< TOML v1.0 */
        BS_FORMAT_INI,      /**< INI with sections */
        BS_FORMAT_XML,      /**< XML (future) */
        BS_FORMAT_AUTO,     /**< Auto-detect from content/extension */
        BS_FORMAT_COUNT     /**< Sentinel — not a real format */
    } bs_format_t;

    /** Format detection result. */
    typedef struct bs_format_detect_result_t
    {
        bs_format_t format;
        int         confidence; /**< 0-100, higher = more confident */
    } bs_format_detect_result_t;

    /** A single top-level key-value entry in the flat v1 JSON model. */
    typedef struct bs_config_entry_t
    {
        const char* key;
        const char* value_json; /**< Raw JSON value (string, number, object, array) */
    } bs_config_entry_t;

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PARSER_CONFIG_FORMAT_TYPES_H */
