#include "bs/adapter/business/scanner.h"
#include "bs/adapter/business/version_check.h"
#include "bs/adapter/business/registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define F_OK 0
#define access _access
#define rmdir _rmdir
#else
#include <unistd.h>
#include <dirent.h>
#endif

/* ─── JSON 解析 helper (极简 cJSON-子集) ───────────────────────────── */

/* 
 * 警告: 这是一个演示级 JSON 解析器, 仅覆盖 manifest.json 需要的一级/二级字段.
 * 生产环境应使用 cJSON / yyjson / rapidjson.
 * 
 * 此处实现: 只解析 biz_id, display_name, sdk_version, fields[], ai_data
 * 的平面 key-value 结构.
 */

#include <ctype.h>

static const char* skip_spaces(const char* p) {
    while (p && *p && isspace((unsigned char)*p)) p++;
    return p;
}

static const char* skip_string(const char* p) {
    if (!p || *p != '"') return NULL;
    p++;
    while (*p) {
        if (*p == '\\' && *(p+1)) p += 2; /* skip escaped char */
        else if (*p == '"') { p++; return p; }
        else p++;
    }
    return NULL;
}

static const char* skip_value(const char* p) {
    /* skips one JSON value, returns pos after it */
    if (!p) return NULL;
    p = skip_spaces(p);
    if (!*p) return NULL;
    if (*p == '"') return skip_string(p);
    if (*p == '{' || *p == '[') {
        int depth = 0;
        char open = *p;
        char close = (open == '{') ? '}' : ']';
        while (*p) {
            if (*p == open) depth++;
            else if (*p == close) { depth--; if (depth == 0) { p++; return p; } }
            else if (*p == '"') { p = skip_string(p); continue; }
            p++;
        }
        return NULL;
    }
    /* number / true / false / null */
    while (*p && !isspace((unsigned char)*p) && *p != ',' && *p != '}' && *p != ']') p++;
    return p;
}

/* ─── 文件读取 ─────────────────────────────────────────────────────── */

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return NULL; }

    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t nread = fread(buf, 1, (size_t)len, f);
    fclose(f);
    if (nread == 0 && len > 0) { free(buf); return NULL; }
    buf[nread] = '\0';
    return buf;
}

/* ─── manifest.json 字段抽取 (简化单遍扫描) ─────────────────────────── */

/*
 * 为了保持代码量可控, 此 scanner 实现只从 JSON 中提取 biz_id,
 * display_name, sdk_version, 并通过上层 manifest_loader.c 做完整解析.
 * 
 * 此处扫描器职责: 发现 business/manifest.json 文件, 读取并验证.
 */

static int ends_with(const char* path, const char* suffix) {
    if (!path || !suffix) return 0;
    size_t plen = strlen(path);
    size_t slen = strlen(suffix);
    if (plen < slen) return 0;
    return strcmp(path + plen - slen, suffix) == 0;
}

static int has_required_fields(const char* json_content) {
    if (!json_content) return 0;
    return strstr(json_content, "\"biz_id\"") != NULL
        && strstr(json_content, "\"display_name\"") != NULL;
}

/* ─── 目录扫描 ─────────────────────────────────────────────────────── */

#ifdef _WIN32
#include <windows.h>

static int scan_dir_win(const char* base_dir, bs_manifest_t** out, size_t* out_count) {
    /* Build wildcard to enumerate subdirectories: base_dir/\* */
    char dir_wildcard[1024];
    size_t base_len = strlen(base_dir);
    if (base_len + 3 > sizeof(dir_wildcard)) return -1;
    /* Ensure separator */
    const char* sep = (base_len > 0 && (base_dir[base_len-1] == '\\' || base_dir[base_len-1] == '/'))
                      ? "" : "\\";
    snprintf(dir_wildcard, sizeof(dir_wildcard), "%s%s*", base_dir, sep);

    size_t capacity = 8;
    bs_manifest_t* manifests = (bs_manifest_t*)calloc(capacity, sizeof(bs_manifest_t));
    size_t count = 0;

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(dir_wildcard, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        *out = NULL;
        *out_count = 0;
        free(manifests);
        return 0;
    }

    do {
        /* Only interested in directories */
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0) continue;

        /* Check for manifest.json inside this subdirectory */
        char manifest_path[1024];
        snprintf(manifest_path, sizeof(manifest_path), "%s%s%s\\manifest.json",
                 base_dir, sep, ffd.cFileName);

        if (_access(manifest_path, F_OK) != 0) continue;

        char* content = read_file(manifest_path);
        if (!content || !has_required_fields(content)) {
            free(content);
            continue;
        }

        if (count >= capacity) {
            capacity *= 2;
            bs_manifest_t* tmp = (bs_manifest_t*)realloc(manifests, capacity * sizeof(bs_manifest_t));
            if (!tmp) { free(content); break; }
            manifests = tmp;
            memset(&manifests[count], 0, (capacity - count) * sizeof(bs_manifest_t));
        }

        strncpy(manifests[count].biz_id, ffd.cFileName, sizeof(manifests[count].biz_id) - 1);
        manifests[count].biz_id[sizeof(manifests[count].biz_id) - 1] = '\0';
        manifests[count].fields = NULL;
        manifests[count].fields_count = 0;

        free(content);
        count++;
    } while (FindNextFileA(hFind, &ffd) != 0);

    FindClose(hFind);

    *out = manifests;
    *out_count = count;
    return 0;
}

#else

static int scan_dir_posix(const char* base_dir, bs_manifest_t** out, size_t* out_count) {
    DIR* dir = opendir(base_dir);
    if (!dir) {
        *out = NULL;
        *out_count = 0;
        return 0;
    }

    size_t capacity = 8;
    bs_manifest_t* manifests = (bs_manifest_t*)calloc(capacity, sizeof(bs_manifest_t));
    size_t count = 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue; /* skip . and .. and hidden */
        if (entry->d_type != DT_DIR && entry->d_type != DT_UNKNOWN) continue;

        char manifest_path[1024];
        size_t blen = strlen(base_dir);
        size_t nlen = strlen(entry->d_name);
        if (blen + 1 + nlen + 14 > sizeof(manifest_path)) continue;
        snprintf(manifest_path, sizeof(manifest_path),
                 "%s/%s/manifest.json", base_dir, entry->d_name);

        struct stat st;
        if (stat(manifest_path, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        char* content = read_file(manifest_path);
        if (!content || !has_required_fields(content)) {
            free(content);
            continue;
        }

        if (count >= capacity) {
            capacity *= 2;
            bs_manifest_t* tmp = (bs_manifest_t*)realloc(manifests,
                capacity * sizeof(bs_manifest_t));
            if (!tmp) { free(content); break; }
            manifests = tmp;
            memset(&manifests[count], 0,
                   (capacity - count) * sizeof(bs_manifest_t));
        }

        strncpy(manifests[count].biz_id, entry->d_name,
                sizeof(manifests[count].biz_id) - 1);
        manifests[count].biz_id[sizeof(manifests[count].biz_id) - 1] = '\0';
        manifests[count].fields = NULL;
        manifests[count].fields_count = 0;

        free(content);
        count++;
    }

    closedir(dir);

    *out = manifests;
    *out_count = count;
    return 0;
}
#endif

int bs_biz_scanner_scan(const char* base_dir,
                         bs_manifest_t** out_manifests,
                         size_t* out_count) {
    if (!base_dir || !out_manifests || !out_count) return -2;

    *out_manifests = NULL;
    *out_count = 0;

#ifdef _WIN32
    return scan_dir_win(base_dir, out_manifests, out_count);
#else
    return scan_dir_posix(base_dir, out_manifests, out_count);
#endif
}

void bs_biz_scanner_free_result(bs_manifest_t* manifests, size_t count) {
    if (!manifests) return;
    for (size_t i = 0; i < count; i++) {
        bs_manifest_destroy(&manifests[i]);
    }
    free(manifests);
}
