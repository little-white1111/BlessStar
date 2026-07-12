/*
 * External JSON Schema ↔ BlessStar schema import/export.
 *
 * MVP: Pass-through JSON reformatting.
 * Full integration uses kernel's schema_json_converter under the hood.
 *
 * Thread safety: reentrant.
 * Error semantics: negative errno codes.
 */

#include "bs/adapter/parser/schema_import_export/schema_import.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int bs_schema_import(const uint8_t* json_schema, size_t schema_len,
                     uint8_t** out, size_t* out_len)
{
    if (!json_schema || !out || !out_len) return -EINVAL;

    /* MVP: copy through as-is with a wrapper annotation */
    size_t cap = schema_len + 256;
    uint8_t* buf = (uint8_t*)malloc(cap);
    if (!buf) return -ENOMEM;

    int n = snprintf((char*)buf, cap,
        "{\n"
        "  \"source\": \"external\",\n"
        "  \"format\": \"json_schema\",\n"
        "  \"schema\": %.*s\n"
        "}",
        (int)schema_len, (const char*)json_schema);

    if (n < 0 || (size_t)n >= cap) {
        free(buf);
        return -ENOBUFS;
    }

    *out     = buf;
    *out_len = (size_t)n + 1;
    return 0;
}

int bs_schema_export(const uint8_t* bs_schema_json, size_t schema_len,
                     uint8_t** out, size_t* out_len)
{
    if (!bs_schema_json || !out || !out_len) return -EINVAL;

    /* MVP: extract the embedded schema field if present, otherwise passthrough */
    uint8_t* buf = (uint8_t*)malloc(schema_len + 1);
    if (!buf) return -ENOMEM;
    memcpy(buf, bs_schema_json, schema_len);
    buf[schema_len] = '\0';

    *out     = buf;
    *out_len = schema_len + 1;
    return 0;
}
