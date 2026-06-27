/* lang_yaml.c — YAML 配置扫描插件 */
#include "bs/biz_introspector/lang_plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <io.h>
#include <process.h>

int lang_yaml_scan(const char* src_dir, char** out_json, size_t* out_len)
{
    if (!src_dir || !out_json) return -1;

    lang_json_buf_t json;
    if (lang_json_init(&json)) return -1;

    lang_json_append(&json, "{\n  \"language\": \"yaml\",\n  \"fields\": [\n");

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "dir /b \"%s\\*.yaml\" \"%s\\*.yml\" 2>nul", src_dir, src_dir);

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

            /* 提取顶层 key（行首非缩进的 key: 模式） */
            const char* p = content;
            while (*p) {
                /* 跳过空行和注释 */
                while (*p == '\n' || *p == '\r') p++;
                if (*p == '#') { while (*p && *p != '\n') p++; continue; }
                if (!*p) break;

                /* 非缩进的 key: 模式 */
                if (*p != ' ' && *p != '\t' && isalpha((unsigned char)*p)) {
                    const char* ks = p;
                    while (*p && *p != ':' && !isspace((unsigned char)*p)) p++;
                    if (*p == ':') {
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
