/* lang_c.c — C/C++ 语言扫描插件
 * 扫描 .c/.h 文件中的 BS_CONFIG_FIELDS() 和 BS_FIELD() 调用。
 * 正则匹配导出配置声明。
 */
#include "bs/biz_introspector/lang_plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <io.h>      /* for _popen, _pclose on Windows */
#include <process.h> /* for _popen, _pclose on Windows */

/* ── 简单的 C 源码扫描 ────────────────────────────────────────────── */

/**
 * 在 C 源码中查找 BS_CONFIG_FIELDS / BS_FIELD 宏调用。
 * 输出 JSON 格式：{ "fields": [{ "key": "...", "type": "..." }] }
 */
static int scan_c_file(const char* filepath, lang_json_buf_t* json)
{
    FILE* f = fopen(filepath, "rb");
    if (!f) return -1;

    /* 读取整个文件 */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize <= 0) { fclose(f); return -1; }
    rewind(f);

    char* content = (char*)malloc((size_t)fsize + 1);
    if (!content) { fclose(f); return -1; }
    size_t read_size = fread(content, 1, (size_t)fsize, f);
    fclose(f);
    content[read_size] = '\0';

    /* 查找 BS_FIELD 宏调用 */
    const char* p = content;
    int first = 1;
    while ((p = strstr(p, "BS_FIELD")) != NULL) {
        p += 8; /* skip "BS_FIELD" */
        while (*p && (*p == '(' || isspace((unsigned char)*p))) p++;
        if (*p != '"') continue;

        p++; /* skip opening quote */
        const char* key_start = p;
        while (*p && *p != '"') p++;
        if (*p != '"') continue;

        /* 提取 key */
        size_t klen = (size_t)(p - key_start);
        char key[256];
        if (klen >= sizeof(key)) klen = sizeof(key) - 1;
        memcpy(key, key_start, klen);
        key[klen] = '\0';

        if (!first) lang_json_append(json, ",\n");
        first = 0;
        lang_json_appendf(json, "    {\"key\": ");
        lang_json_escape(key, json);
        lang_json_appendf(json, ", \"source\": \"%s\"}", filepath);

        p++; /* skip closing quote */
    }

    free(content);
    return first ? -1 : 0; /* -1 = no fields found */
}

int lang_c_scan(const char* src_dir, char** out_json, size_t* out_len)
{
    if (!src_dir || !out_json) return -1;

    lang_json_buf_t json;
    if (lang_json_init(&json)) return -1;

    lang_json_append(&json, "{\n  \"language\": \"c\",\n  \"fields\": [\n");

    /* 简单场景：扫描 src_dir 下的 .c/.h 文件 */
    /* MVP 不递归子目录，仅演示插件框架可用 */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "dir /b \"%s\\*.c\" \"%s\\*.h\" 2>nul", src_dir, src_dir);

    FILE* pipe = _popen(cmd, "r");
    if (pipe) {
        char filename[256];
        while (fgets(filename, sizeof(filename), pipe)) {
            /* 去掉换行 */
            size_t flen = strlen(filename);
            while (flen > 0 && (filename[flen-1] == '\n' || filename[flen-1] == '\r'))
                filename[--flen] = '\0';
            if (flen == 0) continue;

            char fullpath[512];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", src_dir, filename);
            scan_c_file(fullpath, &json);
        }
        _pclose(pipe);
    } else {
        /* fallback: 尝试直接打开一个已知文件 */
        /* 在非 Windows 环境使用 popen + find */
    }

    lang_json_append(&json, "\n  ]\n}\n");

    *out_json = json.buf;
    if (out_len) *out_len = json.len;
    return 0;
}
