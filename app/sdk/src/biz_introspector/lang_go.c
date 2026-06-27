/* lang_go.c — Go 语言扫描插件
 * 扫描 .go 文件中的配置结构体标签和常量定义。
 * MVP 示例：正则匹配 `key:` 或 `json:"key"` 格式。
 */
#include "bs/biz_introspector/lang_plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <io.h>
#include <process.h>

int lang_go_scan(const char* src_dir, char** out_json, size_t* out_len)
{
    if (!src_dir || !out_json) return -1;

    lang_json_buf_t json;
    if (lang_json_init(&json)) return -1;

    lang_json_append(&json, "{\n  \"language\": \"go\",\n  \"fields\": [\n");

    /* MVP: 扫描 .go 文件中的 json tag 字段 */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "dir /b \"%s\\*.go\" 2>nul", src_dir);

    FILE* pipe = _popen(cmd, "r");
    if (pipe) {
        char filename[256];
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

            /* 简单查找 `json:"xxx"` 标签 */
            const char* p = content;
            int first = 1;
            while ((p = strstr(p, "json:\"")) != NULL) {
                p += 6;
                const char* val_start = p;
                while (*p && *p != '"') p++;
                if (*p != '"') break;

                size_t vlen = (size_t)(p - val_start);
                char key[256];
                if (vlen >= sizeof(key)) vlen = sizeof(key) - 1;
                memcpy(key, val_start, vlen);
                key[vlen] = '\0';

                if (!first) lang_json_append(&json, ",\n");
                first = 0;
                lang_json_appendf(&json, "    {\"key\": ");
                lang_json_escape(key, &json);
                lang_json_appendf(&json, ", \"source\": \"%s\", \"tag\": \"json\"}", fullpath);

                p++;
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
