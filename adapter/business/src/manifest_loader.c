#include "bs/adapter/business/manifest_loader.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ─── 极简 JSON 解析器 (单遍扫描, 仅覆盖 manifest.json 所需结构) ──── */

typedef struct json_parse_ctx {
    const char* p;
    const char* end;
    int         error;
} json_parse_ctx_t;

static void jskip_spaces(json_parse_ctx_t* ctx) {
    while (ctx->p < ctx->end && isspace((unsigned char)*ctx->p)) ctx->p++;
}

static int jpeek(json_parse_ctx_t* ctx) {
    jskip_spaces(ctx);
    return (ctx->p < ctx->end) ? (unsigned char)*ctx->p : EOF;
}

static int jnext(json_parse_ctx_t* ctx) {
    int c = jpeek(ctx);
    if (c != EOF) ctx->p++;
    return c;
}

static int jmatch(json_parse_ctx_t* ctx, char c) {
    if (jpeek(ctx) == c) { ctx->p++; return 1; }
    return 0;
}

/* 解析 JSON 字符串到 malloc 缓冲区 */
static char* jparse_string(json_parse_ctx_t* ctx) {
    jskip_spaces(ctx);
    if (ctx->p >= ctx->end || *ctx->p != '"') { ctx->error = 1; return NULL; }
    ctx->p++; /* skip opening quote */

    /* 先计算长度 */
    size_t cap = 64, len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) { ctx->error = 1; return NULL; }

    while (ctx->p < ctx->end) {
        if (*ctx->p == '"') { ctx->p++; buf[len] = '\0'; return buf; }
        if (*ctx->p == '\\' && ctx->p + 1 < ctx->end) {
            ctx->p++;
            switch (*ctx->p) {
                case '"':  buf[len++] = '"';  break;
                case '\\': buf[len++] = '\\'; break;
                case 'n':  buf[len++] = '\n'; break;
                case 'r':  buf[len++] = '\r'; break;
                case 't':  buf[len++] = '\t'; break;
                default:   buf[len++] = *ctx->p; break;
            }
        } else {
            buf[len++] = *ctx->p;
        }
        if (len + 1 >= cap) {
            cap *= 2;
            char* tmp = (char*)realloc(buf, cap);
            if (!tmp) { free(buf); ctx->error = 1; return NULL; }
            buf = tmp;
        }
        ctx->p++;
    }
    free(buf);
    ctx->error = 1;
    return NULL;
}

/* 解析 JSON number / true / false / null 为字符串 */
static char* jparse_value_str(json_parse_ctx_t* ctx) {
    jskip_spaces(ctx);
    if (ctx->p >= ctx->end) return NULL;

    if (*ctx->p == '"') return jparse_string(ctx);

    const char* start = ctx->p;
    while (ctx->p < ctx->end && !isspace((unsigned char)*ctx->p)
           && *ctx->p != ',' && *ctx->p != '}' && *ctx->p != ']')
        ctx->p++;

    size_t len = (size_t)(ctx->p - start);
    char* s = (char*)malloc(len + 1);
    if (!s) return NULL;
    memcpy(s, start, len);
    s[len] = '\0';
    return s;
}

/* 跳过一个 JSON 值 (用于跳过不需要的字段) */
static void jskip_value(json_parse_ctx_t* ctx) {
    jskip_spaces(ctx);
    if (ctx->p >= ctx->end) return;

    switch (*ctx->p) {
    case '"': {
        char* s = jparse_string(ctx);
        free(s);
        return;
    }
    case '{':
    case '[': {
        char open = *ctx->p++;
        char close = (open == '{') ? '}' : ']';
        int depth = 1;
        while (ctx->p < ctx->end && depth > 0) {
            if (*ctx->p == '"') { char* s = jparse_string(ctx); free(s); continue; }
            if (*ctx->p == open) depth++;
            else if (*ctx->p == close) depth--;
            ctx->p++;
        }
        return;
    }
    default:
        /* number / true / false / null */
        while (ctx->p < ctx->end && !isspace((unsigned char)*ctx->p)
               && *ctx->p != ',' && *ctx->p != '}' && *ctx->p != ']')
            ctx->p++;
        return;
    }
}

/* ─── key-value 对搜索 (在 {} 内搜索指定 key) ─────────────────────────── */

static char* find_string_value(json_parse_ctx_t* ctx, const char* key) {
    /* 假设当前在 '{' 之后 */
    while (ctx->p < ctx->end && !ctx->error) {
        if (jmatch(ctx, '}')) return NULL;
        char* k = jparse_string(ctx);
        if (!k) return NULL;
        if (!jmatch(ctx, ':')) { free(k); ctx->error = 1; return NULL; }
        if (strcmp(k, key) == 0) {
            free(k);
            return jparse_string(ctx);
        }
        free(k);
        jskip_value(ctx);
        jmatch(ctx, ',');
    }
    return NULL;
}

static int find_int_value(json_parse_ctx_t* ctx, const char* key, int* out) {
    char* s = find_string_value(ctx, key);
    if (!s) return -1;
    *out = atoi(s);
    free(s);
    return 0;
}

/* ─── fields[] 解析 ──────────────────────────────────────────────────── */

static int parse_fields(json_parse_ctx_t* ctx, bs_manifest_t* out) {
    jskip_spaces(ctx);
    /* 期望看到字段数组开始 */
    if (jpeek(ctx) != '[') { ctx->error = 1; return -1; }
    ctx->p++;

    size_t cap = 8, count = 0;
    bs_manifest_field_t* fields = (bs_manifest_field_t*)calloc(cap, sizeof(bs_manifest_field_t));
    if (!fields) return -2;

    while (ctx->p < ctx->end && !ctx->error) {
        jskip_spaces(ctx);
        if (jmatch(ctx, ']')) break;

        if (!jmatch(ctx, '{')) { ctx->error = 1; free(fields); return -1; }

        bs_manifest_field_t* f = &fields[count];
        while (ctx->p < ctx->end && !ctx->error) {
            jskip_spaces(ctx);
            if (jmatch(ctx, '}')) break;

            char* k = jparse_string(ctx);
            if (!k) { ctx->error = 1; free(fields); return -1; }
            if (!jmatch(ctx, ':')) { free(k); ctx->error = 1; free(fields); return -1; }

            char* v = jparse_value_str(ctx);
            if (strcmp(k, "key") == 0 && v) {
                strncpy(f->key, v, sizeof(f->key) - 1);
            } else if (strcmp(k, "type") == 0 && v) {
                strncpy(f->type, v, sizeof(f->type) - 1);
            } else if (strcmp(k, "default") == 0 && v) {
                strncpy(f->default_str, v, sizeof(f->default_str) - 1);
            } else if (strcmp(k, "description") == 0 && v) {
                strncpy(f->description, v, sizeof(f->description) - 1);
            } else if (strcmp(k, "required") == 0 && v) {
                f->required = (strcmp(v, "true") == 0) ? 1 : 0;
            } else {
                /* 未知字段, 跳过 */
            }
            free(k);
            free(v);
            jmatch(ctx, ',');
        }

        if (!ctx->error && f->key[0] == '\0') {
            /* 缺少必需字段 key */
            ctx->error = 1;
            free(fields);
            return -1;
        }

        count++;
        if (count >= cap) {
            cap *= 2;
            bs_manifest_field_t* tmp = (bs_manifest_field_t*)realloc(fields, cap * sizeof(bs_manifest_field_t));
            if (!tmp) { free(fields); return -2; }
            fields = tmp;
            memset(&fields[count], 0, (cap - count) * sizeof(bs_manifest_field_t));
        }
        jmatch(ctx, ',');
    }

    out->fields = fields;
    out->fields_count = count;
    return 0;
}

/* ─── ai_data.config_labels 解析 ──────────────────────────────────────── */

static int parse_config_labels(json_parse_ctx_t* ctx, bs_manifest_ai_labels_t* out) {
    memset(out, 0, sizeof(*out));
    jskip_spaces(ctx);
    if (jpeek(ctx) != '{') { jskip_value(ctx); return 0; }
    ctx->p++; /* skip { */

    size_t cap = 8, count = 0;
    char** keys = (char**)calloc(cap, sizeof(char*));
    char** vals = (char**)calloc(cap, sizeof(char*));
    if (!keys || !vals) { free(keys); free(vals); return -2; }

    while (ctx->p < ctx->end && !ctx->error) {
        jskip_spaces(ctx);
        if (jmatch(ctx, '}')) break;

        char* k = jparse_string(ctx);
        if (!k) break;
        if (!jmatch(ctx, ':')) { free(k); break; }
        char* v = jparse_string(ctx);

        keys[count] = k;
        vals[count] = v ? v : strdup("");
        count++;

        if (count >= cap) {
            cap *= 2;
            char** tk = (char**)realloc(keys, cap * sizeof(char*));
            char** tv = (char**)realloc(vals, cap * sizeof(char*));
            if (!tk || !tv) { free(keys); free(vals); return -2; }
            keys = tk; vals = tv;
        }
        jmatch(ctx, ',');
    }

    out->keys = keys;
    out->values = vals;
    out->count = count;
    return 0;
}

/* ─── ai_data.inverted_index 解析 ────────────────────────────────────── */

static int parse_inverted_index(json_parse_ctx_t* ctx, bs_manifest_inv_entry_t** out, size_t* out_count) {
    *out = NULL; *out_count = 0;
    jskip_spaces(ctx);
    if (jpeek(ctx) != '[') { jskip_value(ctx); return 0; }
    ctx->p++;

    size_t cap = 8, count = 0;
    bs_manifest_inv_entry_t* arr = (bs_manifest_inv_entry_t*)calloc(cap, sizeof(bs_manifest_inv_entry_t));
    if (!arr) return -2;

    while (ctx->p < ctx->end && !ctx->error) {
        jskip_spaces(ctx);
        if (jmatch(ctx, ']')) break;
        if (!jmatch(ctx, '{')) { ctx->error = 1; free(arr); return -1; }

        bs_manifest_inv_entry_t* e = &arr[count];
        while (ctx->p < ctx->end && !ctx->error) {
            jskip_spaces(ctx);
            if (jmatch(ctx, '}')) break;

            char* k = jparse_string(ctx);
            if (!k) break;
            if (!jmatch(ctx, ':')) { free(k); break; }

            if (strcmp(k, "keyword") == 0) {
                char* v = jparse_string(ctx);
                if (v) { strncpy(e->keyword, v, sizeof(e->keyword) - 1); free(v); }
            } else if (strcmp(k, "config_keys") == 0) {
                /* 解析字符串数组 */
                jskip_spaces(ctx);
                if (jmatch(ctx, '[')) {
                    size_t kcap = 4, kcount = 0;
                    char** karr = (char**)calloc(kcap, sizeof(char*));
                    while (ctx->p < ctx->end && !ctx->error) {
                        jskip_spaces(ctx);
                        if (jmatch(ctx, ']')) break;
                        char* kv = jparse_string(ctx);
                        if (!kv) break;
                        karr[kcount++] = kv;
                        if (kcount >= kcap) { kcap *= 2; karr = (char**)realloc(karr, kcap * sizeof(char*)); }
                        jmatch(ctx, ',');
                    }
                    e->config_keys = karr;
                    e->keys_count = kcount;
                } else {
                    jskip_value(ctx);
                }
            } else {
                jskip_value(ctx);
            }
            free(k);
            jmatch(ctx, ',');
        }
        count++;
        if (count >= cap) {
            cap *= 2;
            bs_manifest_inv_entry_t* tmp = (bs_manifest_inv_entry_t*)realloc(arr, cap * sizeof(bs_manifest_inv_entry_t));
            if (!tmp) { free(arr); return -2; }
            arr = tmp;
            memset(&arr[count], 0, (cap - count) * sizeof(bs_manifest_inv_entry_t));
        }
        jmatch(ctx, ',');
    }

    *out = arr;
    *out_count = count;
    return 0;
}

/* ─── ai_data.domain_shards 解析 ─────────────────────────────────────── */

static int parse_domain_shards(json_parse_ctx_t* ctx, bs_manifest_domain_shard_t** out, size_t* out_count) {
    *out = NULL; *out_count = 0;
    jskip_spaces(ctx);
    if (jpeek(ctx) != '[') { jskip_value(ctx); return 0; }
    ctx->p++;

    size_t cap = 4, count = 0;
    bs_manifest_domain_shard_t* arr = (bs_manifest_domain_shard_t*)calloc(cap, sizeof(bs_manifest_domain_shard_t));
    if (!arr) return -2;

    while (ctx->p < ctx->end && !ctx->error) {
        jskip_spaces(ctx);
        if (jmatch(ctx, ']')) break;
        if (!jmatch(ctx, '{')) { ctx->error = 1; free(arr); return -1; }

        bs_manifest_domain_shard_t* s = &arr[count];
        while (ctx->p < ctx->end && !ctx->error) {
            jskip_spaces(ctx);
            if (jmatch(ctx, '}')) break;

            char* k = jparse_string(ctx);
            if (!k) break;
            if (!jmatch(ctx, ':')) { free(k); break; }

            if (strcmp(k, "domain_name") == 0) {
                char* v = jparse_string(ctx);
                if (v) { strncpy(s->domain_name, v, sizeof(s->domain_name) - 1); free(v); }
            } else if (strcmp(k, "domain_description") == 0) {
                char* v = jparse_string(ctx);
                if (v) { strncpy(s->domain_description, v, sizeof(s->domain_description) - 1); free(v); }
            } else if (strcmp(k, "keywords") == 0) {
                jskip_spaces(ctx);
                if (jmatch(ctx, '[')) {
                    size_t kcap = 4, kcount = 0;
                    char** karr = (char**)calloc(kcap, sizeof(char*));
                    while (ctx->p < ctx->end && !ctx->error) {
                        jskip_spaces(ctx);
                        if (jmatch(ctx, ']')) break;
                        char* kv = jparse_string(ctx);
                        if (!kv) break;
                        karr[kcount++] = kv;
                        if (kcount >= kcap) { kcap *= 2; karr = (char**)realloc(karr, kcap * sizeof(char*)); }
                        jmatch(ctx, ',');
                    }
                    s->keywords = karr;
                    s->keywords_count = kcount;
                } else { jskip_value(ctx); }
            } else { jskip_value(ctx); }
            free(k);
            jmatch(ctx, ',');
        }
        count++;
        if (count >= cap) {
            cap *= 2;
            bs_manifest_domain_shard_t* tmp = (bs_manifest_domain_shard_t*)realloc(arr, cap * sizeof(bs_manifest_domain_shard_t));
            if (!tmp) { free(arr); return -2; }
            arr = tmp;
            memset(&arr[count], 0, (cap - count) * sizeof(bs_manifest_domain_shard_t));
        }
        jmatch(ctx, ',');
    }

    *out = arr;
    *out_count = count;
    return 0;
}

/* ─── ai_data.skill_routes 解析 ──────────────────────────────────────── */

static int parse_skill_routes(json_parse_ctx_t* ctx, bs_manifest_skill_route_t** out, size_t* out_count) {
    *out = NULL; *out_count = 0;
    jskip_spaces(ctx);
    if (jpeek(ctx) != '[') { jskip_value(ctx); return 0; }
    ctx->p++;

    size_t cap = 4, count = 0;
    bs_manifest_skill_route_t* arr = (bs_manifest_skill_route_t*)calloc(cap, sizeof(bs_manifest_skill_route_t));
    if (!arr) return -2;

    while (ctx->p < ctx->end && !ctx->error) {
        jskip_spaces(ctx);
        if (jmatch(ctx, ']')) break;
        if (!jmatch(ctx, '{')) { ctx->error = 1; free(arr); return -1; }

        bs_manifest_skill_route_t* r = &arr[count];
        while (ctx->p < ctx->end && !ctx->error) {
            jskip_spaces(ctx);
            if (jmatch(ctx, '}')) break;

            char* k = jparse_string(ctx);
            if (!k) break;
            if (!jmatch(ctx, ':')) { free(k); break; }

            if (strcmp(k, "prefix") == 0) {
                char* v = jparse_string(ctx);
                if (v) { strncpy(r->prefix, v, sizeof(r->prefix) - 1); free(v); }
            } else if (strcmp(k, "description") == 0) {
                char* v = jparse_string(ctx);
                if (v) { strncpy(r->description, v, sizeof(r->description) - 1); free(v); }
            } else if (strcmp(k, "priority") == 0) {
                char* v = jparse_value_str(ctx);
                if (v) { r->priority = atoi(v); free(v); }
            } else if (strcmp(k, "tool_chain") == 0) {
                jskip_spaces(ctx);
                if (jmatch(ctx, '[')) {
                    size_t kcap = 4, kcount = 0;
                    char** karr = (char**)calloc(kcap, sizeof(char*));
                    while (ctx->p < ctx->end && !ctx->error) {
                        jskip_spaces(ctx);
                        if (jmatch(ctx, ']')) break;
                        char* kv = jparse_string(ctx);
                        if (!kv) break;
                        karr[kcount++] = kv;
                        if (kcount >= kcap) { kcap *= 2; karr = (char**)realloc(karr, kcap * sizeof(char*)); }
                        jmatch(ctx, ',');
                    }
                    r->tool_chain = karr;
                    r->tool_chain_count = kcount;
                } else { jskip_value(ctx); }
            } else { jskip_value(ctx); }
            free(k);
            jmatch(ctx, ',');
        }
        count++;
        if (count >= cap) {
            cap *= 2;
            bs_manifest_skill_route_t* tmp = (bs_manifest_skill_route_t*)realloc(arr, cap * sizeof(bs_manifest_skill_route_t));
            if (!tmp) { free(arr); return -2; }
            arr = tmp;
            memset(&arr[count], 0, (cap - count) * sizeof(bs_manifest_skill_route_t));
        }
        jmatch(ctx, ',');
    }

    *out = arr;
    *out_count = count;
    return 0;
}

/* ─── ai_data 顶层解析 ───────────────────────────────────────────────── */

static int parse_ai_data(json_parse_ctx_t* ctx, bs_manifest_ai_data_t* out) {
    memset(out, 0, sizeof(*out));

    if (jpeek(ctx) != '{') { jskip_value(ctx); return 0; }
    ctx->p++; /* skip { */

    while (ctx->p < ctx->end && !ctx->error) {
        jskip_spaces(ctx);
        if (jmatch(ctx, '}')) return 0;

        char* k = jparse_string(ctx);
        if (!k) return 0;
        if (!jmatch(ctx, ':')) { free(k); ctx->error = 1; return -1; }

        if (strcmp(k, "config_labels") == 0) {
            parse_config_labels(ctx, &out->config_labels);
        } else if (strcmp(k, "inverted_index") == 0) {
            parse_inverted_index(ctx, &out->inverted_index, &out->inverted_index_count);
        } else if (strcmp(k, "domain_shards") == 0) {
            parse_domain_shards(ctx, &out->domain_shards, &out->domain_shards_count);
        } else if (strcmp(k, "skill_routes") == 0) {
            parse_skill_routes(ctx, &out->skill_routes, &out->skill_routes_count);
        } else {
            jskip_value(ctx);
        }
        free(k);
        jmatch(ctx, ',');
    }

    return ctx->error ? -1 : 0;
}

/* ─── 完整 manifest JSON 解析 ────────────────────────────────────────── */

int bs_manifest_load_from_json(const char* json, bs_manifest_t* out) {
    if (!json || !out) return -1;

    memset(out, 0, sizeof(*out));

    json_parse_ctx_t ctx;
    ctx.p = json;
    ctx.end = json + strlen(json);
    ctx.error = 0;

    /* 期望顶层对象 */
    jskip_spaces(&ctx);
    if (jnext(&ctx) != '{') return -1;

    int found_biz_id = 0;
    int found_display = 0;

    while (ctx.p < ctx.end && !ctx.error) {
        jskip_spaces(&ctx);
        if (jmatch(&ctx, '}')) break;

        char* k = jparse_string(&ctx);
        if (!k) break;
        if (!jmatch(&ctx, ':')) { free(k); ctx.error = 1; break; }

        if (strcmp(k, "biz_id") == 0) {
            char* v = jparse_string(&ctx);
            if (v) { strncpy(out->biz_id, v, sizeof(out->biz_id) - 1); found_biz_id = 1; free(v); }
        } else if (strcmp(k, "display_name") == 0) {
            char* v = jparse_string(&ctx);
            if (v) { strncpy(out->display_name, v, sizeof(out->display_name) - 1); found_display = 1; free(v); }
        } else if (strcmp(k, "sdk_version") == 0) {
            char* v = jparse_string(&ctx);
            if (v) { strncpy(out->sdk_version, v, sizeof(out->sdk_version) - 1); free(v); }
        } else if (strcmp(k, "normalizer_lib_path") == 0) {
            char* v = jparse_string(&ctx);
            if (v) { strncpy(out->normalizer_lib_path, v, sizeof(out->normalizer_lib_path) - 1); free(v); }
        } else if (strcmp(k, "fields") == 0) {
            parse_fields(&ctx, out);
        } else if (strcmp(k, "ai_data") == 0) {
            parse_ai_data(&ctx, &out->ai_data);
        } else {
            jskip_value(&ctx);
        }
        free(k);
        jmatch(&ctx, ',');
    }

    if (!found_biz_id || !found_display) {
        bs_manifest_destroy(out);
        return -1;
    }

    return ctx.error ? -1 : 0;
}

int bs_manifest_load_from_file(const char* file_path, bs_manifest_t* out) {
    if (!file_path || !out) return -1;

    FILE* f = fopen(file_path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return -1; }

    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) { fclose(f); return -3; }

    size_t nread = fread(buf, 1, (size_t)len, f);
    fclose(f);
    if (nread == 0 && len > 0) { free(buf); return -1; }
    buf[nread] = '\0';

    int rc = bs_manifest_load_from_json(buf, out);
    free(buf);
    return rc ? -2 : 0;
}
