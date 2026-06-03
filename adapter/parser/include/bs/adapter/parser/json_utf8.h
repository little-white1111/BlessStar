#ifndef BS_ADAPTER_PARSER_JSON_UTF8_H
#define BS_ADAPTER_PARSER_JSON_UTF8_H

/*
 * C-ST-7 contract block:
 * Thread safety: pure functions; no shared state.
 * Error semantics: returns 0/1 for decode/validation; no BsStatus.
 * Platform notes: N/A.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /** Returns 1 if code point is valid for JSON string content (no lone surrogates). */
    int bs_json_utf8_codepoint_valid(unsigned int cp);

    /**
     * Decode one UTF-8 code point from [p, end). Advances *p on success.
     * @return 1 ok, 0 invalid
     */
    int bs_json_utf8_decode_advance(const char** p, const char* end, unsigned int* out_cp);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PARSER_JSON_UTF8_H */
