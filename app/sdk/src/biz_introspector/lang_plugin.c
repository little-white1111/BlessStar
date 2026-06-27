/* lang_plugin.c — 多语言内省插件注册表实现 */
#include "bs/biz_introspector/lang_plugin.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* ── 全局注册表 ────────────────────────────────────────────────────── */
lang_plugin_t g_lang_plugins[LANG_PLUGIN_MAX];
size_t        g_lang_plugin_count = 0;

int lang_plugin_register(const lang_plugin_t* plugin)
{
    if (!plugin || !plugin->language || !plugin->scan) return -1;
    if (g_lang_plugin_count >= LANG_PLUGIN_MAX) return -1;

    /* 去重：同名插件不重复注册 */
    for (size_t i = 0; i < g_lang_plugin_count; i++) {
        if (strcmp(g_lang_plugins[i].language, plugin->language) == 0) {
            g_lang_plugins[i] = *plugin; /* 覆盖 */
            return 0;
        }
    }

    g_lang_plugins[g_lang_plugin_count++] = *plugin;
    return 0;
}

const lang_plugin_t* lang_plugin_find(const char* language)
{
    if (!language) return NULL;
    for (size_t i = 0; i < g_lang_plugin_count; i++) {
        if (strcmp(g_lang_plugins[i].language, language) == 0) {
            return &g_lang_plugins[i];
        }
    }
    return NULL;
}

/* ── JSON 辅助工具 ────────────────────────────────────────────────── */

int lang_json_init(lang_json_buf_t* b)
{
    b->cap = 4096;
    b->buf = (char*)malloc(b->cap);
    if (!b->buf) return -1;
    b->buf[0] = '\0';
    b->len = 0;
    return 0;
}

int lang_json_grow(lang_json_buf_t* b, size_t needed)
{
    if (b->len + needed < b->cap) return 0;
    size_t new_cap = b->cap * 2;
    while (b->len + needed >= new_cap) new_cap *= 2;
    char* new_buf = (char*)realloc(b->buf, new_cap);
    if (!new_buf) return -1;
    b->buf = new_buf;
    b->cap = new_cap;
    return 0;
}

int lang_json_append(lang_json_buf_t* b, const char* s)
{
    size_t slen = strlen(s);
    if (lang_json_grow(b, slen + 1)) return -1;
    memcpy(b->buf + b->len, s, slen);
    b->len += slen;
    b->buf[b->len] = '\0';
    return 0;
}

int lang_json_appendf(lang_json_buf_t* b, const char* fmt, ...)
{
    va_list args, args2;
    va_start(args, fmt);
    va_copy(args2, args);
    int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (needed < 0) { va_end(args2); return -1; }
    size_t n = (size_t)needed;
    if (lang_json_grow(b, n + 1)) { va_end(args2); return -1; }
    vsnprintf(b->buf + b->len, b->cap - b->len, fmt, args2);
    va_end(args2);
    b->len += n;
    b->buf[b->len] = '\0';
    return 0;
}

void lang_json_destroy(lang_json_buf_t* b)
{
    free(b->buf);
    b->buf = NULL;
    b->len = 0;
    b->cap = 0;
}

void lang_json_escape(const char* s, lang_json_buf_t* b)
{
    lang_json_append(b, "\"");
    while (s && *s) {
        char c = *s;
        switch (c) {
        case '"':  lang_json_append(b, "\\\""); break;
        case '\\': lang_json_append(b, "\\\\"); break;
        case '\n': lang_json_append(b, "\\n");  break;
        case '\r': lang_json_append(b, "\\r");  break;
        case '\t': lang_json_append(b, "\\t");  break;
        default:
            if ((unsigned char)c < 0x20)
                lang_json_appendf(b, "\\u%04x", (unsigned char)c);
            else
                lang_json_appendf(b, "%c", c);
            break;
        }
        s++;
    }
    lang_json_append(b, "\"");
}
