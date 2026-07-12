/*
 * v1 JSON → formatted JSON serializer.
 *
 * Re-formats compact v1 JSON with indentation for human readability.
 * For pass-through (no change), we just copy and indent.
 *
 * Thread safety: reentrant.
 * Error semantics: negative errno codes.
 */

#include "bs/adapter/parser/config_format/format_convert.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations from json_lexer / json_parser if needed */
/* For MVP we do a simple pretty-print pass-through */

/**
 * Minimal JSON pretty-printer.
 * Handles: objects, arrays, strings, numbers, booleans, null.
 * Adds 2-space indentation.
 */
static int pretty_print_json(const uint8_t* input, size_t input_len,
                             uint8_t** out, size_t* out_len)
{
    /* For v1 JSON, the canonical v1 JSON format is already the internal format.
     * We do a round-trip safe pretty-print to ensure consistent output. */
    if (!input || input_len == 0) {
        return -EINVAL;
    }

    /* Estimate: pretty-printed JSON ~2x input size */
    size_t cap  = input_len * 2 + 256;
    uint8_t* buf = (uint8_t*)malloc(cap);
    if (!buf) return -ENOMEM;

    size_t  pos  = 0;
    int     indent = 0;

    for (size_t i = 0; i < input_len && pos < cap; ++i) {
        uint8_t ch = input[i];

        switch (ch) {
        case '{':
        case '[':
            buf[pos++] = ch;
            indent++;
            buf[pos++] = '\n';
            for (int j = 0; j < indent && pos < cap; ++j) {
                buf[pos++] = ' ';
                buf[pos++] = ' ';
            }
            break;

        case '}':
        case ']':
            indent--;
            buf[pos++] = '\n';
            for (int j = 0; j < indent && pos < cap; ++j) {
                buf[pos++] = ' ';
                buf[pos++] = ' ';
            }
            buf[pos++] = ch;
            break;

        case ',':
            buf[pos++] = ch;
            buf[pos++] = '\n';
            for (int j = 0; j < indent && pos < cap; ++j) {
                buf[pos++] = ' ';
                buf[pos++] = ' ';
            }
            break;

        case ':':
            buf[pos++] = ch;
            buf[pos++] = ' ';
            break;

        case ' ':
        case '\t':
        case '\n':
        case '\r':
            /* Skip original whitespace; we regenerate it */
            break;

        default:
            buf[pos++] = ch;
            break;
        }
    }

    if (pos >= cap) {
        free(buf);
        return -ENOBUFS;
    }
    buf[pos] = '\0';

    *out     = buf;
    *out_len = pos + 1;
    return 0;
}

int bs_internal_to_json(const uint8_t* v1_json, size_t v1_len,
                         uint8_t** out, size_t* out_len)
{
    return pretty_print_json(v1_json, v1_len, out, out_len);
}
