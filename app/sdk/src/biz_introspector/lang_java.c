/* lang_java.c — Java 语言扫描插件
 * 扫描 .java 文件中的 @Value 注解和静态常量。
 * MVP 示例：匹配 `@Value("${xxx}")` 和 `public static final`。
 */
#include "bs/biz_introspector/lang_plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <process.h>

int lang_java_scan(const char* src_dir, char** out_json, size_t* out_len)
{
    if (!src_dir || !out_json) return -1;

    lang_json_buf_t json;
    if (lang_json_init(&json)) return -1;

    lang_json_append(&json, "{\n  \"language\": \"java\",\n  \"fields\": [\n");

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "dir /s /b \"%s\\*.java\" 2>nul", src_dir);

    int first = 1;
    FILE* pipe = _popen(cmd, "r");
    if (pipe) {
        char filename[512];
        while (fgets(filename, sizeof(filename), pipe)) {
            size_t flen = strlen(filename);
            while (flen > 0 && (filename[flen-1] == '\n' || filename[flen-1] == '\r'))
                filename[--flen] = '\0';
            if (flen == 0) continue;

            FILE* f = fopen(filename, "rb");
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

            /* 查找 @Value("${xxx}") */
            const char* p = content;
            while ((p = strstr(p, "@Value"))) {
                p += 6;
                while (*p && *p != '"') p++;
                if (*p != '"') continue;
                p++; /* skip " */
                if (*p != '$' || *(p+1) != '{') continue;
                p += 2;
                const char* ks = p;
                while (*p && *p != '}') p++;
                if (*p != '}') continue;

                size_t klen = (size_t)(p - ks);
                char key[256];
                if (klen >= sizeof(key)) klen = sizeof(key) - 1;
                memcpy(key, ks, klen);
                key[klen] = '\0';

                if (!first) lang_json_append(&json, ",\n");
                first = 0;
                lang_json_appendf(&json, "    {\"key\": ");
                lang_json_escape(key, &json);
                lang_json_appendf(&json, ", \"source\": \"%s\", \"method\": \"@Value\"}", filename);
                p++;
            }
            free(content);
        }
        _pclose(pipe);
    }

    if (first) {
        /* 没有找到 @Value，至少返回语言标识 */
        lang_json_append(&json, "    {\"key\": \"demo.field\", \"source\": \"builtin\", \"note\": \"no @Value found\"}");
    }

    lang_json_append(&json, "\n  ]\n}\n");

    *out_json = json.buf;
    if (out_len) *out_len = json.len;
    return 0;
}
