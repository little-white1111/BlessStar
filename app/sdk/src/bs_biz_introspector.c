/* bs_biz_introspect: static analysis → biz_semantic_index.json
 * MVP: scans source files for public symbols and builds index. */
#include <bs/bs_biz_introspector.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#define mkdir_p(path) _mkdir(path)
#else
#include <sys/stat.h>
#define mkdir_p(path) mkdir(path, 0755)
#endif

/* ── JSON builder helpers (inline, zero external deps) ── */
typedef struct {
    char*  buf;
    size_t len;
    size_t cap;
} bi_json_buf_t;

static int bi_json_buf_init(bi_json_buf_t* b)
{
    b->cap = 4096;
    b->buf = (char*)malloc(b->cap);
    if (!b->buf) return -1;
    b->buf[0] = '\0';
    b->len = 0;
    return 0;
}

static int bi_json_buf_grow(bi_json_buf_t* b, size_t needed)
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

static int bi_json_buf_append(bi_json_buf_t* b, const char* s)
{
    size_t slen = strlen(s);
    if (bi_json_buf_grow(b, slen + 1)) return -1;
    memcpy(b->buf + b->len, s, slen);
    b->len += slen;
    b->buf[b->len] = '\0';
    return 0;
}

static int bi_json_buf_appendf(bi_json_buf_t* b, const char* fmt, ...)
{
    va_list args, args2;
    va_start(args, fmt);
    va_copy(args2, args);
    int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (needed < 0) { va_end(args2); return -1; }
    size_t n = (size_t)needed;
    if (bi_json_buf_grow(b, n + 1)) { va_end(args2); return -1; }
    vsnprintf(b->buf + b->len, b->cap - b->len, fmt, args2);
    va_end(args2);
    b->len += n;
    b->buf[b->len] = '\0';
    return 0;
}

static void bi_json_buf_destroy(bi_json_buf_t* b) { free(b->buf); b->buf = NULL; }

static void bi_json_escape(const char* s, bi_json_buf_t* b)
{
    bi_json_buf_append(b, "\"");
    while (*s) {
        char c = *s;
        switch (c) {
        case '"':  bi_json_buf_append(b, "\\\""); break;
        case '\\': bi_json_buf_append(b, "\\\\"); break;
        case '\n': bi_json_buf_append(b, "\\n"); break;
        case '\r': bi_json_buf_append(b, "\\r"); break;
        case '\t': bi_json_buf_append(b, "\\t"); break;
        default:
            if ((unsigned char)c < 0x20)
                bi_json_buf_appendf(b, "\\u%04x", (unsigned char)c);
            else
                bi_json_buf_appendf(b, "%c", c);
            break;
        }
        s++;
    }
    bi_json_buf_append(b, "\"");
}

static void get_iso_timestamp(char* buf, size_t sz)
{
    time_t t = time(NULL);
    struct tm* tm_ptr;
#ifdef _WIN32
    struct tm tm_buf;
    tm_ptr = gmtime_s(&tm_buf, &t) ? NULL : &tm_buf;
#else
    struct tm tm_buf;
    tm_ptr = gmtime_r(&t, &tm_buf);
#endif
    if (tm_ptr)
        strftime(buf, sz, "%Y-%m-%dT%H:%M:%SZ", tm_ptr);
    else
        snprintf(buf, sz, "unknown");
}

static const char* category_name(bs_biz_symbol_category_t cat)
{
    switch (cat) {
    case BS_BIZ_SYMBOL_CLASS:     return "class";
    case BS_BIZ_SYMBOL_FUNCTION:  return "function";
    case BS_BIZ_SYMBOL_INTERFACE: return "interface";
    case BS_BIZ_SYMBOL_STRUCT:    return "struct";
    case BS_BIZ_SYMBOL_ENUM:      return "enum";
    default:                      return "unknown";
    }
}

/* ── Simplified C/C++ scanner ──────────────────────────────────────── */
typedef struct {
    char* name;
    char* file;
    int   line;
    bs_biz_symbol_category_t category;
    char* ai_hint;
} bi_symbol_t;

#define BI_MAX_SYMBOLS 1024

typedef struct {
    bi_symbol_t symbols[BI_MAX_SYMBOLS];
    int          count;
} bi_scan_ctx_t;

static char* extract_ident(const char* line, const char* keyword)
{
    const char* pos = line;
    while (*pos == ' ' || *pos == '\t') pos++;

    if (strncmp(pos, keyword, strlen(keyword)) != 0) return NULL;
    pos += strlen(keyword);
    while (*pos == ' ' || *pos == '\t') pos++;
    if (*pos == '*' || *pos == '&') pos++;

    const char* start = pos;
    while ((*pos >= 'a' && *pos <= 'z') ||
           (*pos >= 'A' && *pos <= 'Z') ||
           (*pos >= '0' && *pos <= '9') ||
           *pos == '_') pos++;
    if (pos == start) return NULL;

    size_t len = (size_t)(pos - start);
    char* name = (char*)malloc(len + 1);
    if (!name) return NULL;
    memcpy(name, start, len);
    name[len] = '\0';
    return name;
}

static bool is_source_file(const char* fname)
{
    const char* ext = strrchr(fname, '.');
    if (!ext) return false;
    return (strcmp(ext, ".c") == 0 || strcmp(ext, ".cpp") == 0 ||
            strcmp(ext, ".cc") == 0 || strcmp(ext, ".cxx") == 0 ||
            strcmp(ext, ".h") == 0 || strcmp(ext, ".hpp") == 0 ||
            strcmp(ext, ".java") == 0 || strcmp(ext, ".kt") == 0 ||
            strcmp(ext, ".py") == 0);
}

static const char* g_keywords[] = {
    "void", "int", "int32_t", "int64_t", "uint32_t", "uint64_t",
    "double", "float", "char", "bool", "bool_t", "size_t", "void_t",
    NULL
};

static int scan_dir(const char* path, bi_scan_ctx_t* ctx, int recursion_depth)
{
    if (recursion_depth > 10 || ctx->count >= BI_MAX_SYMBOLS) return 0;

#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*", path);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;

        char full[2048];
        snprintf(full, sizeof(full), "%s\\%s", path, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            /* Skip hidden/build dirs */
            if (fd.cFileName[0] == '.') continue;
            if (strcmp(fd.cFileName, "build") == 0 ||
                strcmp(fd.cFileName, "node_modules") == 0 ||
                strcmp(fd.cFileName, "dist") == 0 ||
                strcmp(fd.cFileName, ".git") == 0)
                continue;
            scan_dir(full, ctx, recursion_depth + 1);
        } else if (is_source_file(fd.cFileName)) {
            FILE* f = fopen(full, "r");
            if (!f) continue;

            char line[2048];
            int line_no = 0;
            while (fgets(line, sizeof(line), f) && ctx->count < BI_MAX_SYMBOLS) {
                line_no++;

                char* name = NULL;
                bs_biz_symbol_category_t cat = BS_BIZ_SYMBOL_FUNCTION;

                /* Try all type keywords */
                for (int k = 0; g_keywords[k] && !name; k++) {
                    name = extract_ident(line, g_keywords[k]);
                    if (name) cat = BS_BIZ_SYMBOL_FUNCTION;
                }

                if (!name) {
                    name = extract_ident(line, "struct");
                    if (name) cat = BS_BIZ_SYMBOL_STRUCT;
                }
                if (!name) {
                    name = extract_ident(line, "enum");
                    if (name) cat = BS_BIZ_SYMBOL_ENUM;
                }
                if (!name) {
                    name = extract_ident(line, "typedef");
                    if (name) cat = BS_BIZ_SYMBOL_STRUCT;
                }

                if (name && strlen(name) > 1) {
                    bi_symbol_t* sym = &ctx->symbols[ctx->count];
                    sym->name     = name;
                    sym->file     = strdup(fd.cFileName);
                    sym->line     = line_no;
                    sym->category = cat;
                    sym->ai_hint  = strdup("");
                    ctx->count++;
                } else {
                    free(name);
                }
            }
            fclose(f);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#endif
    return 0;
}

int bs_biz_introspect(const bs_biz_introspect_config_t* config,
                       char** out_index_json, size_t* out_len)
{
    if (!config || !out_index_json) return -1;

    bi_scan_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    if (config->src_dir && config->src_dir[0]) {
        scan_dir(config->src_dir, &ctx, 0);
    }

    bi_json_buf_t b;
    if (bi_json_buf_init(&b)) return -1;

    char ts[64];
    get_iso_timestamp(ts, sizeof(ts));

    bi_json_buf_append(&b, "{\n");
    bi_json_buf_appendf(&b, "\"version\":\"1.0\",\n");
    bi_json_buf_appendf(&b, "\"generated_at\":");
    bi_json_escape(ts, &b);
    bi_json_buf_appendf(&b, ",\n\"language\":");
    bi_json_escape(config->language ? config->language : "C", &b);
    bi_json_buf_appendf(&b, ",\n\"symbol_count\":%d,\n", ctx.count);
    bi_json_buf_append(&b, "\"symbols\":[\n");

    for (int i = 0; i < ctx.count; i++) {
        if (i > 0) bi_json_buf_append(&b, ",");
        bi_symbol_t* s = &ctx.symbols[i];
        bi_json_buf_append(&b, "\n    {\n");
        bi_json_buf_append(&b, "      \"name\":");
        bi_json_escape(s->name, &b);
        bi_json_buf_appendf(&b, ",\n      \"category\":\"%s\"", category_name(s->category));
        bi_json_buf_append(&b, ",\n      \"file\":");
        bi_json_escape(s->file, &b);
        bi_json_buf_appendf(&b, ",\n      \"line\":%d", s->line);
        bi_json_buf_append(&b, ",\n      \"ai_hint\":");
        bi_json_escape(s->ai_hint ? s->ai_hint : "", &b);
        bi_json_buf_append(&b, "\n    }");

        free(s->name);
        free(s->file);
        free(s->ai_hint);
    }
    bi_json_buf_append(&b, "\n  ]\n}\n");

    *out_index_json = b.buf;
    if (out_len) *out_len = b.len;
    return 0;
}

void bs_biz_introspect_free(char* json)
{
    free(json);
}
