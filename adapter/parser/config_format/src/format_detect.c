/*
 * Format auto-detection via content bytes and file extension.
 *
 * Thread safety: reentrant, no global state.
 * Error semantics: returns BS_FORMAT_JSON with low confidence on failure.
 * Platform notes: N/A.
 */

#include "bs/adapter/parser/config_format/format_convert.h"

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* MSVC doesn't have strcasecmp; provide portable fallback. */
#if defined(_MSC_VER)
#define strcasecmp _stricmp
#endif

/* ─── Content-based heuristics ───────────────────────────────────── */

/**
 * Check if data starts with a YAML "---" document separator.
 */
static int is_yaml_header(const uint8_t* data, size_t len)
{
    if (len < 3) return 0;
    return (data[0] == '-' && data[1] == '-' && data[2] == '-');
}

/**
 * Check if data looks like YAML:
 *   - Has "key: value" pattern on the first non-blank line.
 */
static int is_yaml_content(const uint8_t* data, size_t len)
{
    size_t i = 0;
    while (i < len && (data[i] == ' ' || data[i] == '\t' || data[i] == '\n' || data[i] == '\r'))
        i++;
    if (i >= len) return 0;
    /* Skip first word (key) */
    while (i < len && (isalnum(data[i]) || data[i] == '_' || data[i] == '-'))
        i++;
    while (i < len && (data[i] == ' ' || data[i] == '\t'))
        i++;
    /* YAML uses ": " or ":" followed by value */
    if (i < len && data[i] == ':') {
        i++;
        /* Must be followed by space, newline, or end — not '=' (TOML) or ':' (already counted) */
        if (i >= len || data[i] == ' ' || data[i] == '\t' || data[i] == '\n' || data[i] == '\r')
            return 1;
    }
    return 0;
}

/**
 * Check if data looks like TOML:
 *   - First non-whitespace line matches a TOML key = value or [section] pattern.
 *   - For [section] headers, disambiguate from INI: TOML uses " = " (spaces around '=').
 */
static int is_toml_content(const uint8_t* data, size_t len)
{
    if (len < 3) return 0;
    /* TOML often starts with a bare key or [section] */
    size_t i = 0;
    /* Skip leading whitespace */
    while (i < len && (data[i] == ' ' || data[i] == '\t' || data[i] == '\n' || data[i] == '\r'))
        i++;
    if (i >= len) return 0;
    /* Check for [section] header */
    if (data[i] == '[') {
        /* Disambiguate from INI: look for " = " (spaces around '=') after header */
        size_t nl = i + 1;
        while (nl < len && data[nl] != '\n')
            nl++;
        if (nl + 3 < len) {
            size_t v = nl + 1;
            while (v < len && (data[v] == ' ' || data[v] == '\t'))
                v++;
            while (v + 2 < len && data[v] != '=')
                v++;
            if (v + 2 < len && data[v] == '=' &&
                data[v-1] == ' ' && data[v+1] == ' ')
                return 1; /* TOML style: key = value */
            return 0; /* likely INI style: key=value */
        }
        return 0; /* no content after header to disambiguate */
    }
    /* Check for key = value pattern: alphanumeric chars followed by '=' */
    while (i < len && (isalnum(data[i]) || data[i] == '_' || data[i] == '.'))
        i++;
    while (i < len && (data[i] == ' ' || data[i] == '\t'))
        i++;
    return (i < len && data[i] == '=');
}

/**
 * Check if data looks like INI:
 *   - Has [section] headers and key=value lines (no spaces around '=').
 */
static int is_ini_content(const uint8_t* data, size_t len)
{
    if (len < 3) return 0;
    size_t i = 0;
    /* Skip leading whitespace */
    while (i < len && (data[i] == ' ' || data[i] == '\t' || data[i] == '\n' || data[i] == '\r'))
        i++;
    if (i >= len) return 0;
    /* INI typically starts with a [section] or a comment ';' */
    if (data[i] == '[' || data[i] == ';') {
        /* Disambiguate TOML vs INI: INI uses key=value without spaces around '=' */
        size_t eq = i + 1;
        while (eq < len && data[eq] != '\n')
            eq++;
        if (eq + 3 < len) {
            /* Check if the line after [section] has '=' without leading space */
            size_t v = eq + 1;
            while (v < len && (data[v] == ' ' || data[v] == '\t'))
                v++;
            /* Find next '=' */
            while (v < len && data[v] != '=' && data[v] != '\n' && data[v] != '[')
                v++;
            if (v < len && data[v] == '=') {
                /* INI style: check no space before '=' */
                if (v > 0 && data[v-1] != ' ')
                    return 1;
                /* If there IS a space before '=', it could be TOML — still return 1
                 * for MVP; the detection order handles the preference. */
                return 1;
            }
        }
        return 1; /* [section] alone is enough for MVP */
    }
    return 0;
}

/**
 * Check if data looks like JSON:
 *   - Starts with '{' or '['.
 */
static int is_json_content(const uint8_t* data, size_t len)
{
    if (len < 1) return 0;
    size_t i = 0;
    /* Skip BOM or whitespace */
    if (len >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
        i = 3;
    while (i < len && (data[i] == ' ' || data[i] == '\t' || data[i] == '\n' || data[i] == '\r'))
        i++;
    if (i >= len) return 0;
    if (data[i] == '{') return 1;
    /* '[' could be JSON array or TOML/INI section.
     * Disambiguate: if the next non-whitespace char after '['
     * is a letter, it's likely a TOML/INI section header. */
    if (data[i] == '[') {
        size_t j = i + 1;
        while (j < len && (data[j] == ' ' || data[j] == '\t'))
            j++;
        if (j < len && ((data[j] >= 'a' && data[j] <= 'z') ||
                        (data[j] >= 'A' && data[j] <= 'Z')))
            return 0; /* looks like [section] header, not JSON */
        return 1;
    }
    return 0;
}

/* ─── Extension-based heuristics ──────────────────────────────────── */

static const struct {
    const char* ext;
    bs_format_t format;
} extension_map[] = {
    {".json", BS_FORMAT_JSON},
    {".yaml", BS_FORMAT_YAML},
    {".yml",  BS_FORMAT_YAML},
    {".toml", BS_FORMAT_TOML},
    {".ini",  BS_FORMAT_INI},
    {".conf", BS_FORMAT_INI},
    {".cfg",  BS_FORMAT_INI},
    {NULL,    BS_FORMAT_JSON}
};

static bs_format_t detect_by_extension(const char* filename_hint)
{
    if (!filename_hint) return BS_FORMAT_JSON;

    const char* dot = strrchr(filename_hint, '.');
    if (!dot) return BS_FORMAT_JSON;

    for (int i = 0; extension_map[i].ext != NULL; ++i) {
        if (strcasecmp(dot, extension_map[i].ext) == 0) {
            return extension_map[i].format;
        }
    }
    return BS_FORMAT_JSON;
}

/* ─── Public API ──────────────────────────────────────────────────── */

bs_format_detect_result_t bs_format_detect(const uint8_t* data, size_t len,
                                            const char* filename_hint)
{
    bs_format_detect_result_t result = {BS_FORMAT_JSON, 30};

    if (!data || len == 0) {
        /* No content — rely solely on extension hint */
        if (filename_hint) {
            result.format = detect_by_extension(filename_hint);
            if (result.format != BS_FORMAT_JSON)
                result.confidence = 60;
        } else {
            result.confidence = 0;
        }
        return result;
    }

    /* 1. Content-based detection (higher priority) */
    if (is_json_content(data, len)) {
        result = (bs_format_detect_result_t){BS_FORMAT_JSON, 90};
    } else if (is_yaml_header(data, len) || is_yaml_content(data, len)) {
        result = (bs_format_detect_result_t){BS_FORMAT_YAML, 95};
    } else if (is_toml_content(data, len)) {
        result = (bs_format_detect_result_t){BS_FORMAT_TOML, 85};
    } else if (is_ini_content(data, len)) {
        result = (bs_format_detect_result_t){BS_FORMAT_INI, 85};
    }

    /* 2. Extension-based — used to confirm or override low-conf results */
    if (result.confidence < 70 && filename_hint) {
        bs_format_t ext_fmt = detect_by_extension(filename_hint);
        if (ext_fmt != result.format) {
            result.format      = ext_fmt;
            result.confidence  = 60;
        } else {
            result.confidence = (result.confidence + 80) / 2;
        }
    }

    return result;
}
