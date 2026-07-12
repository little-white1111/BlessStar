/*
 * ADR-全链路接通 — Format Converter unified entry point.
 *
 * Thread safety: reentrant, no global state.
 * Error semantics: negative errno codes.
 * Platform notes: N/A.
 */

#include "bs/adapter/parser/config_format/format_convert.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations of per-format serializers. */
extern int bs_internal_to_json(const uint8_t* v1_json, size_t v1_len,
                                uint8_t** out, size_t* out_len);
extern int bs_internal_to_yaml(const uint8_t* v1_json, size_t v1_len,
                                uint8_t** out, size_t* out_len);
extern int bs_internal_to_toml(const uint8_t* v1_json, size_t v1_len,
                                uint8_t** out, size_t* out_len);
extern int bs_internal_to_ini(const uint8_t* v1_json, size_t v1_len,
                               uint8_t** out, size_t* out_len);

int bs_format_convert(const uint8_t* v1_json, size_t v1_len,
                      bs_format_t target_fmt,
                      uint8_t** out, size_t* out_len)
{
    if (!v1_json || !out || !out_len) {
        return -EINVAL;
    }

    *out     = NULL;
    *out_len = 0;

    switch (target_fmt) {
    case BS_FORMAT_JSON:
        return bs_internal_to_json(v1_json, v1_len, out, out_len);
    case BS_FORMAT_YAML:
        return bs_internal_to_yaml(v1_json, v1_len, out, out_len);
    case BS_FORMAT_TOML:
        return bs_internal_to_toml(v1_json, v1_len, out, out_len);
    case BS_FORMAT_INI:
        return bs_internal_to_ini(v1_json, v1_len, out, out_len);
    case BS_FORMAT_XML:
        /* XML support not yet implemented */
        return -ENOTSUP;
    case BS_FORMAT_AUTO:
    case BS_FORMAT_COUNT:
    default:
        return -EINVAL;
    }
}
