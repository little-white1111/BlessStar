/**
 * lang_plugin — 多语言内省插件公共接口
 *
 * 每个语言插件实现 lang_scan_fn_t，bs_biz_introspector 通过
 * lang_scan_fn[] 注册表调用不同语言的扫描器。
 *
 * 生命周期：注册 → scan() → free_json()
 */
#ifndef BS_LANG_PLUGIN_H
#define BS_LANG_PLUGIN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 插件接口 ─────────────────────────────────────────────────────── */

/**
 * 语言扫描函数。
 * @param src_dir   源码目录
 * @param out_json  输出 JSON 字符串（调用方负责 free）
 * @param out_len   输出 JSON 长度
 * @return 0 = 成功，-1 = 失败
 */
typedef int (*lang_scan_fn_t)(const char* src_dir, char** out_json, size_t* out_len);

/**
 * 语言插件描述符。
 */
typedef struct {
    const char* language;    /* "c", "go", "python", "java", "yaml" */
    const char* extensions;  /* ".c.h", ".go", ".py", ".java", ".yaml.yml" */
    lang_scan_fn_t scan;     /* 扫描函数指针 */
} lang_plugin_t;

/* ── 全局注册表 ────────────────────────────────────────────────────── */

/** 最大插件数 */
#define LANG_PLUGIN_MAX 16

/** 全局插件注册表（定义在 bs_biz_introspector.c 中） */
extern lang_plugin_t g_lang_plugins[LANG_PLUGIN_MAX];
extern size_t        g_lang_plugin_count;

/**
 * 注册一个语言插件。
 * 每个语言插件在初始化时调用此函数。
 */
int lang_plugin_register(const lang_plugin_t* plugin);

/**
 * 根据语言名称查找插件。
 */
const lang_plugin_t* lang_plugin_find(const char* language);

/* ── JSON 辅助工具（插件内部使用）────────────────────────────────── */
typedef struct {
    char*  buf;
    size_t len;
    size_t cap;
} lang_json_buf_t;

int  lang_json_init(lang_json_buf_t* b);
int  lang_json_grow(lang_json_buf_t* b, size_t needed);
int  lang_json_append(lang_json_buf_t* b, const char* s);
int  lang_json_appendf(lang_json_buf_t* b, const char* fmt, ...);
void lang_json_destroy(lang_json_buf_t* b);
void lang_json_escape(const char* s, lang_json_buf_t* b);

#ifdef __cplusplus
}
#endif

#endif /* BS_LANG_PLUGIN_H */
