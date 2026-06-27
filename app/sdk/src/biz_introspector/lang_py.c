/* lang_py.c — Python 语言扫描插件
 * 扫描 .py 文件中的配置字典/类变量定义。
 * MVP 示例：匹配 `KEY = ` 或 `"key": ` 模式。
 */
#include "bs/biz_introspector/lang_plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <io.h>
#include <process.h>

int lang_py_scan(const char* src_dir, char** out_json, size_t* out_len)
{
    if (!src_dir || !out_json) return -1;

    lang_json_buf_t json;
    if (lang_json_init(&json)) return -1;

    lang_json_append(&json, "{\n  \"language\": \"python\",\n  \"fields\": [\n");

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "dir /b \"%s\\*.py\" 2>nul", src_dir);

    FILE* pipe = _popen(cmd, "r");
    if (pipe) {
        char filename[256];
        int first = 1;
        while (fgets(filename, sizeof(filename), pipe)) {
            size_t flen = strlen(filename);
            while (flen > 0 && (filename[flen-1] == '\n' || filename[flen-1] == '\r'))
                filename[--flen] = '\0';
            if (flen == 0) continue;

            char fullpath[512];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", src_dir, filename);

            FILE* f = fopen(fullpath, "rb");
            if (!f) continue;
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            if (fsize <= 0) { fclose(f); continue; }
            rewind(f);
            char* content = (char*)malloc((size_t)fsize + 1);
            if (!content) { fclose(f); continue; }
            size_t rsize = fread(content, 1, (size_t)fsize, f);
            fclose(f);
            content[rsize] = '\0';

            /* 匹配大写常量定义和字典 key */
            const char* p = content;
            while (*p) {
                /* 大写字母开头后跟 = 的常量 */
                if (isupper((unsigned char)*p)) {
                    const char* ks = p;
                    while (*p && (isalnum((unsigned char)*p) || *p == '_')) p++;
                    if (p > ks && *p == '=') {
                        size_t klen = (size_t)(p - ks);
                        char key[256];
                        if (klen >= sizeof(key)) klen = sizeof(key) - 1;
                        memcpy(key, ks, klen);
                        key[klen] = '\0';

                        if (!first) lang_json_append(&json, ",\n");
                        first = 0;
                        lang_json_appendf(&json, "    {\"key\": ");
                        lang_json_escape(key, &json);
                        lang_json_appendf(&json, ", \"source\": \"%s\"}", fullpath);
                    }
                } else {
                    p++;
                }
            }
            free(content);
        }
        _pclose(pipe);
    }

    lang_json_append(&json, "\n  ]\n}\n");

    *out_json = json.buf;
    if (out_len) *out_len = json.len;
    return 0;
}
