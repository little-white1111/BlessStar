/* agent_indexer: Schema + Gate chain → domain_knowledge.json / constraint_knowledge.json / field_semantics.json */
/* Also supports compact (pipe-delimited) format output */
#include <bs/agent_indexer.h>
#include <bs/kernel/schema/schema_registry.h>
#include <bs/kernel/gate_chain/gate_chain_serialize.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir_p(path) _mkdir(path)
#else
#include <sys/stat.h>
#define mkdir_p(path) mkdir(path, 0755)
#endif

/* ── JSON builder helpers (inline, no external dep) ── */
typedef struct {
    char*  buf;
    size_t len;
    size_t cap;
} ai_json_buf_t;

static int ai_json_buf_init(ai_json_buf_t* b)
{
    b->cap = 4096;
    b->buf = (char*)malloc(b->cap);
    if (!b->buf) return -1;
    b->buf[0] = '\0';
    b->len = 0;
    return 0;
}

static int ai_json_buf_grow(ai_json_buf_t* b, size_t needed)
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

static int ai_json_buf_append(ai_json_buf_t* b, const char* s)
{
    size_t slen = strlen(s);
    if (ai_json_buf_grow(b, slen + 1)) return -1;
    memcpy(b->buf + b->len, s, slen);
    b->len += slen;
    b->buf[b->len] = '\0';
    return 0;
}

static int ai_json_buf_appendf(ai_json_buf_t* b, const char* fmt, ...)
{
    va_list args, args2;
    va_start(args, fmt);
    va_copy(args2, args);
    int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (needed < 0) { va_end(args2); return -1; }
    size_t n = (size_t)needed;
    if (ai_json_buf_grow(b, n + 1)) { va_end(args2); return -1; }
    vsnprintf(b->buf + b->len, b->cap - b->len, fmt, args2);
    va_end(args2);
    b->len += n;
    b->buf[b->len] = '\0';
    return 0;
}

static void ai_json_buf_destroy(ai_json_buf_t* b) { free(b->buf); b->buf = NULL; }

static void ai_json_escape(const char* s, ai_json_buf_t* b)
{
    ai_json_buf_append(b, "\"");
    while (*s) {
        char c = *s;
        switch (c) {
        case '"':  ai_json_buf_append(b, "\\\""); break;
        case '\\': ai_json_buf_append(b, "\\\\"); break;
        case '\n': ai_json_buf_append(b, "\\n"); break;
        case '\r': ai_json_buf_append(b, "\\r"); break;
        case '\t': ai_json_buf_append(b, "\\t"); break;
        default:
            if ((unsigned char)c < 0x20)
                ai_json_buf_appendf(b, "\\u%04x", (unsigned char)c);
            else
                ai_json_buf_appendf(b, "%c", c);
            break;
        }
        s++;
    }
    ai_json_buf_append(b, "\"");
}

/* ── Timestamp ────────────────────────────────────────────────────── */
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

/* ── Schema type to string ────────────────────────────────────────── */
static const char* schema_type_str(bs_schema_type_t t)
{
    switch (t) {
    case BS_SCHEMA_TYPE_STR:  return "str";
    case BS_SCHEMA_TYPE_I32:  return "i32";
    case BS_SCHEMA_TYPE_I64:  return "i64";
    case BS_SCHEMA_TYPE_F64:  return "f64";
    case BS_SCHEMA_TYPE_BOOL: return "bool";
    case BS_SCHEMA_TYPE_ARR:  return "arr";
    case BS_SCHEMA_TYPE_OBJ:  return "obj";
    case BS_SCHEMA_TYPE_ENUM: return "enum";
    default: return "unknown";
    }
}

/* ── Ensure directory exists ──────────────────────────────────────── */
static int ensure_dir(const char* path)
{
    /* Create intermediate directories one by one */
    char* tmp = strdup(path);
    if (!tmp) return -1;
    int ret = 0;
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char saved = *p;
            *p = '\0';
            mkdir_p(tmp);
            *p = saved;
        }
    }
    mkdir_p(tmp);
    free(tmp);
    return 0;
}

/* ── Write file ───────────────────────────────────────────────────── */
static int write_file(const char* path, const char* content)
{
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    return (written == len) ? 0 : -1;
}

/* ── Free string array ────────────────────────────────────────────── */
static void free_str_array(char** arr, size_t count)
{
    if (!arr) return;
    for (size_t i = 0; i < count; i++) free(arr[i]);
    free(arr);
}

/* ── Infer widget from schema type ──────────────────────────────────── */
static const char* infer_widget(bs_schema_type_t type)
{
    switch (type) {
    case BS_SCHEMA_TYPE_BOOL: return "checkbox";
    case BS_SCHEMA_TYPE_I32:
    case BS_SCHEMA_TYPE_I64:
    case BS_SCHEMA_TYPE_F64:  return "number";
    case BS_SCHEMA_TYPE_ENUM: return "select";
    case BS_SCHEMA_TYPE_OBJ:  return "group";
    case BS_SCHEMA_TYPE_ARR:  return "repeatable";
    default: return "input";
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * Compact (pipe-delimited) serialization: field_semantics
 * field_key|type|required|widget|ai_hint
 * ══════════════════════════════════════════════════════════════════════ */
typedef struct {
    ai_json_buf_t* buf;
    int            count;
} compact_sem_ctx_t;

static void compact_sem_foreach_cb(const bs_schema_entry_t* entry, void* userdata)
{
    compact_sem_ctx_t* ctx = (compact_sem_ctx_t*)userdata;
    ai_json_buf_t* b = ctx->buf;

    for (size_t i = 0; i < entry->root_count; i++) {
        const bs_schema_field_def_t* fd = &entry->root_fields[i];
        ai_json_buf_append(b, fd->name);
        ai_json_buf_append(b, "|");
        ai_json_buf_append(b, schema_type_str(fd->type));
        ai_json_buf_append(b, "|");
        ai_json_buf_append(b, fd->required ? "true" : "false");
        ai_json_buf_append(b, "|");
        ai_json_buf_append(b, infer_widget(fd->type));
        ai_json_buf_append(b, "|");
        ai_json_buf_append(b, fd->ai_hint ? fd->ai_hint : "");
        ai_json_buf_append(b, "\n");
        ctx->count++;
    }
}

static int build_field_semantics_compact(ai_json_buf_t* b,
                                          bs_schema_registry_t* reg)
{
    ai_json_buf_append(b, "# field_semantics.compact\n");
    ai_json_buf_append(b, "field_key|type|required|widget|ai_hint\n");

    compact_sem_ctx_t ctx;
    ctx.buf = b;
    ctx.count = 0;
    bs_schema_foreach(reg, compact_sem_foreach_cb, &ctx);
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Compact: domain_knowledge
 * domain_name|field_count|field_list
 * ══════════════════════════════════════════════════════════════════════ */
typedef struct {
    ai_json_buf_t* buf;
    int            count;
} compact_domain_ctx_t;

static void compact_domain_foreach_cb(const bs_schema_entry_t* entry, void* userdata)
{
    compact_domain_ctx_t* ctx = (compact_domain_ctx_t*)userdata;
    ai_json_buf_t* b = ctx->buf;

    const char* domain = entry->schema_id ? entry->schema_id : "unknown";
    ai_json_buf_append(b, domain);
    ai_json_buf_append(b, "|");

    /* field_count */
    char count_str[16];
    snprintf(count_str, sizeof(count_str), "%zu", entry->root_count);
    ai_json_buf_append(b, count_str);
    ai_json_buf_append(b, "|");

    /* field_list: comma separated */
    for (size_t i = 0; i < entry->root_count; i++) {
        if (i > 0) ai_json_buf_append(b, ",");
        ai_json_buf_append(b, entry->root_fields[i].name);
    }
    ai_json_buf_append(b, "\n");
    ctx->count++;
}

static int build_domain_knowledge_compact(ai_json_buf_t* b,
                                           bs_schema_registry_t* reg)
{
    ai_json_buf_append(b, "# domain_knowledge.compact\n");
    ai_json_buf_append(b, "domain_name|field_count|field_list\n");

    compact_domain_ctx_t ctx;
    ctx.buf = b;
    ctx.count = 0;
    bs_schema_foreach(reg, compact_domain_foreach_cb, &ctx);
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Compact: constraint_knowledge
 * gate_id|scenario|field_key|op|value|layer|sub_category
 * ══════════════════════════════════════════════════════════════════════ */
static int build_constraint_knowledge_compact(ai_json_buf_t* b,
                                               const bs_gate_chain_t* chain,
                                               const bs_agent_index_config_t* config)
{
    (void)config;
    ai_json_buf_append(b, "# constraint_knowledge.compact\n");
    ai_json_buf_append(b, "gate_id|scenario|field_key|op|value|layer|sub_category\n");

    if (!chain) return 0;
    for (size_t i = 0; i < chain->node_count; i++) {
        const bs_gate_node_t* n = &chain->nodes[i];
        ai_json_buf_append(b, n->id ? n->id : "");
        ai_json_buf_append(b, "|");
        ai_json_buf_append(b, n->type ? n->type : "");
        ai_json_buf_append(b, "|");
        ai_json_buf_append(b, n->field_key ? n->field_key : "");
        ai_json_buf_append(b, "|");
        ai_json_buf_append(b, n->op ? n->op : "");
        ai_json_buf_append(b, "|");
        ai_json_buf_append(b, n->value ? n->value : "");
        ai_json_buf_append(b, "|");
        ai_json_buf_appendf(b, "%zu", n->layer);
        ai_json_buf_append(b, "|");
        /* sub_category is not directly in gate_node, use type as fallback */
        ai_json_buf_append(b, n->type ? n->type : "");
        ai_json_buf_append(b, "\n");
    }
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Domain knowledge builder (traverse schema registry)
 * ══════════════════════════════════════════════════════════════════════ */
typedef struct {
    ai_json_buf_t* buf;
    int            field_count;
} domain_build_ctx_t;

static void domain_foreach_cb(const bs_schema_entry_t* entry, void* userdata)
{
    domain_build_ctx_t* ctx = (domain_build_ctx_t*)userdata;
    ai_json_buf_t* b = ctx->buf;

    if (ctx->field_count > 0) ai_json_buf_append(b, ",");
    ai_json_buf_appendf(b, "\n    {\n      \"domain\":");
    ai_json_escape(entry->schema_id ? entry->schema_id : "unknown", b);
    ai_json_buf_append(b, ",\n      \"description\":");
    ai_json_escape(entry->ui_meta.description ? entry->ui_meta.description : "",
                   b);
    ai_json_buf_append(b, ",\n      \"fields\":[\n");

    for (size_t i = 0; i < entry->root_count; i++) {
        if (i > 0) ai_json_buf_append(b, ",");
        const bs_schema_field_def_t* fd = &entry->root_fields[i];
        ai_json_buf_append(b, "\n        {");
        ai_json_buf_append(b, "\"key\":");
        ai_json_escape(fd->name, b);
        ai_json_buf_append(b, ",\"type\":");
        ai_json_escape(schema_type_str(fd->type), b);
        ai_json_buf_appendf(b, ",\"required\":%s",
                            fd->required ? "true" : "false");

        /* ai_hint */
        if (fd->ai_hint) {
            ai_json_buf_append(b, ",\"ai_hint\":");
            ai_json_escape(fd->ai_hint, b);
        }

        /* range */
        if (fd->range.has_min || fd->range.has_max) {
            ai_json_buf_append(b, ",\"range\":{");
            if (fd->range.has_min)
                ai_json_buf_appendf(b, "\"min\":%g", fd->range.min);
            if (fd->range.has_min && fd->range.has_max)
                ai_json_buf_append(b, ",");
            if (fd->range.has_max)
                ai_json_buf_appendf(b, "\"max\":%g", fd->range.max);
            ai_json_buf_append(b, "}");
        }

        /* enum */
        if (fd->enum_values) {
            ai_json_buf_append(b, ",\"enum\":[");
            int first = 1;
            for (int j = 0; fd->enum_values[j]; j++) {
                if (!first) ai_json_buf_append(b, ",");
                ai_json_escape(fd->enum_values[j], b);
                first = 0;
            }
            ai_json_buf_append(b, "]");
        }

        /* ui_meta */
        if (fd->ui_label) {
            ai_json_buf_append(b, ",\"ui_meta\":{\"widget\":");
            /* Infer widget from type */
            ai_json_buf_appendf(b, "%s,\"label\":", infer_widget(fd->type));
            ai_json_escape(fd->ui_label, b);
            ai_json_buf_append(b, "}");
        }
        ai_json_buf_append(b, "}");
    }
    ai_json_buf_append(b, "\n      ]\n    }");
    ctx->field_count++;
}

static int build_domain_knowledge(ai_json_buf_t* b,
                                   const char* business_name,
                                   bs_schema_registry_t* reg)
{
    char ts[64];
    get_iso_timestamp(ts, sizeof(ts));

    ai_json_buf_append(b, "{\n");
    ai_json_buf_appendf(b, "\"version\":\"1.0\",\n");
    ai_json_buf_appendf(b, "\"business_name\":");
    ai_json_escape(business_name ? business_name : "default", b);
    ai_json_buf_appendf(b, ",\n\"generated_at\":");
    ai_json_escape(ts, b);
    ai_json_buf_append(b, ",\n\"domains\":[");

    domain_build_ctx_t ctx;
    ctx.buf = b;
    ctx.field_count = 0;
    bs_schema_foreach(reg, domain_foreach_cb, &ctx);

    ai_json_buf_append(b, "\n  ]\n}\n");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Constraint knowledge builder (from gate chain)
 * ══════════════════════════════════════════════════════════════════════ */
static int build_constraint_knowledge(ai_json_buf_t* b,
                                       const bs_gate_chain_t* chain)
{
    if (!chain || chain->node_count == 0) {
        ai_json_buf_append(b, "{\"version\":\"1.0\",\"generated_at\":\"\",\"gates\":[]}\n");
        return 0;
    }

    char ts[64];
    get_iso_timestamp(ts, sizeof(ts));

    ai_json_buf_append(b, "{\n");
    ai_json_buf_appendf(b, "\"version\":\"1.0\",\n");
    ai_json_buf_appendf(b, "\"generated_at\":");
    ai_json_escape(ts, b);
    ai_json_buf_append(b, ",\n\"gates\":[\n");

    for (size_t i = 0; i < chain->node_count; i++) {
        if (i > 0) ai_json_buf_append(b, ",");
        const bs_gate_node_t* n = &chain->nodes[i];
        ai_json_buf_append(b, "\n    {");
        int first = 1;

        if (n->id) {
            ai_json_buf_append(b, "\"id\":");
            ai_json_escape(n->id, b);
            first = 0;
        }
        if (n->type) {
            if (!first) ai_json_buf_append(b, ",");
            ai_json_buf_append(b, "\"type\":");
            ai_json_escape(n->type, b);
            first = 0;
        }
        if (n->field_key) {
            if (!first) ai_json_buf_append(b, ",");
            ai_json_buf_append(b, "\"field_key\":");
            ai_json_escape(n->field_key, b);
            first = 0;
        }
        if (n->op) {
            if (!first) ai_json_buf_append(b, ",");
            ai_json_buf_append(b, "\"op\":");
            ai_json_escape(n->op, b);
            first = 0;
        }
        if (n->value) {
            if (!first) ai_json_buf_append(b, ",");
            ai_json_buf_append(b, "\"value\":");
            ai_json_escape(n->value, b);
            first = 0;
        }
        if (n->child_count > 0 && n->child_ids) {
            if (!first) ai_json_buf_append(b, ",");
            ai_json_buf_append(b, "\"children\":[");
            for (size_t j = 0; j < n->child_count; j++) {
                if (j > 0) ai_json_buf_append(b, ",");
                ai_json_escape(n->child_ids[j], b);
            }
            ai_json_buf_append(b, "]");
            first = 0;
        }
        if (n->do_count > 0 && n->do_ids) {
            if (!first) ai_json_buf_append(b, ",");
            ai_json_buf_append(b, "\"do\":[");
            for (size_t j = 0; j < n->do_count; j++) {
                if (j > 0) ai_json_buf_append(b, ",");
                ai_json_escape(n->do_ids[j], b);
            }
            ai_json_buf_append(b, "]");
            first = 0;
        }
        ai_json_buf_append(b, "}");
    }
    ai_json_buf_append(b, "\n  ]\n}\n");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Field semantics builder (from schema registry)
 * ══════════════════════════════════════════════════════════════════════ */
typedef struct {
    ai_json_buf_t* buf;
    int            field_count;
} sem_build_ctx_t;

static void sem_foreach_cb(const bs_schema_entry_t* entry, void* userdata)
{
    sem_build_ctx_t* ctx = (sem_build_ctx_t*)userdata;
    ai_json_buf_t* b = ctx->buf;

    for (size_t i = 0; i < entry->root_count; i++) {
        const bs_schema_field_def_t* fd = &entry->root_fields[i];

        if (ctx->field_count > 0) ai_json_buf_append(b, ",");
        ai_json_buf_append(b, "\n    {");
        ai_json_buf_append(b, "\"key\":");
        ai_json_escape(fd->name, b);
        ai_json_buf_append(b, ",\"type\":");
        ai_json_escape(schema_type_str(fd->type), b);

        /* description from ai_hint */
        if (fd->ai_hint) {
            ai_json_buf_append(b, ",\"description\":");
            ai_json_escape(fd->ai_hint, b);
        }

        /* constraints */
        ai_json_buf_append(b, ",\"constraints\":[");
        int cfirst = 1;
        if (fd->range.has_min || fd->range.has_max) {
            char range_str[128] = {0};
            if (fd->range.has_min && fd->range.has_max)
                snprintf(range_str, sizeof(range_str), "[%g,%g]",
                         fd->range.min, fd->range.max);
            else if (fd->range.has_min)
                snprintf(range_str, sizeof(range_str), "[%g,)",
                         fd->range.min);
            else
                snprintf(range_str, sizeof(range_str), "(,%g]",
                         fd->range.max);
            if (!cfirst) ai_json_buf_append(b, ",");
            ai_json_escape(range_str, b);
            cfirst = 0;
        }
        if (fd->required) {
            if (!cfirst) ai_json_buf_append(b, ",");
            ai_json_escape("required", b);
            cfirst = 0;
        }
        ai_json_buf_append(b, "]");
        ai_json_buf_append(b, "}");
        ctx->field_count++;
    }
}

static int build_field_semantics(ai_json_buf_t* b,
                                  bs_schema_registry_t* reg)
{
    char ts[64];
    get_iso_timestamp(ts, sizeof(ts));

    ai_json_buf_append(b, "{\n");
    ai_json_buf_appendf(b, "\"version\":\"1.0\",\n");
    ai_json_buf_appendf(b, "\"generated_at\":");
    ai_json_escape(ts, b);
    ai_json_buf_append(b, ",\n\"fields\":[");

    sem_build_ctx_t ctx;
    ctx.buf = b;
    ctx.field_count = 0;
    bs_schema_foreach(reg, sem_foreach_cb, &ctx);

    ai_json_buf_append(b, "\n  ]\n}\n");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * Public API
 * ══════════════════════════════════════════════════════════════════════ */
int bs_agent_index_export(
    bs_schema_registry_t*  reg,
    const bs_gate_chain_t* chain,
    const bs_agent_index_config_t* config)
{
    if (!reg || !config) return -1;

    ensure_dir(config->output_dir);

    /* Determine if compact output is requested */
    int do_compact = (config->format != NULL) && (strcmp(config->format, "compact") == 0);

    char path[512];

    /* 1) domain_knowledge.json */
    {
        ai_json_buf_t dk;
        if (ai_json_buf_init(&dk)) return -1;
        build_domain_knowledge(&dk, config->business_name, reg);

        snprintf(path, sizeof(path), "%s/domain_knowledge.json", config->output_dir);
        int ret = write_file(path, dk.buf);
        ai_json_buf_destroy(&dk);
        if (ret) return ret;
    }

    /* 1c) domain_knowledge.compact (optional) */
    if (do_compact) {
        ai_json_buf_t dkc;
        if (ai_json_buf_init(&dkc)) return -1;
        build_domain_knowledge_compact(&dkc, reg);

        snprintf(path, sizeof(path), "%s/domain_knowledge.compact", config->output_dir);
        int ret = write_file(path, dkc.buf);
        ai_json_buf_destroy(&dkc);
        if (ret) return ret;
    }

    /* 2) constraint_knowledge.json */
    if (config->include_gate_chain && chain) {
        ai_json_buf_t ck;
        if (ai_json_buf_init(&ck)) return -1;
        build_constraint_knowledge(&ck, chain);

        snprintf(path, sizeof(path), "%s/constraint_knowledge.json", config->output_dir);
        int ret = write_file(path, ck.buf);
        ai_json_buf_destroy(&ck);
        if (ret) return ret;
    }

    /* 2c) constraint_knowledge.compact (optional) */
    if (do_compact && config->include_gate_chain && chain) {
        ai_json_buf_t ckc;
        if (ai_json_buf_init(&ckc)) return -1;
        build_constraint_knowledge_compact(&ckc, chain, config);

        snprintf(path, sizeof(path), "%s/constraint_knowledge.compact", config->output_dir);
        int ret = write_file(path, ckc.buf);
        ai_json_buf_destroy(&ckc);
        if (ret) return ret;
    }

    /* 3) field_semantics.json */
    {
        ai_json_buf_t fs;
        if (ai_json_buf_init(&fs)) return -1;
        build_field_semantics(&fs, reg);

        snprintf(path, sizeof(path), "%s/field_semantics.json", config->output_dir);
        int ret = write_file(path, fs.buf);
        ai_json_buf_destroy(&fs);
        if (ret) return ret;
    }

    /* 3c) field_semantics.compact (optional) */
    if (do_compact) {
        ai_json_buf_t fsc;
        if (ai_json_buf_init(&fsc)) return -1;
        build_field_semantics_compact(&fsc, reg);

        snprintf(path, sizeof(path), "%s/field_semantics.compact", config->output_dir);
        int ret = write_file(path, fsc.buf);
        ai_json_buf_destroy(&fsc);
        if (ret) return ret;
    }

    /* ── OPT-08: 4) biz_semantic_index.json ── */
    if (config->include_biz_index) {
        ai_json_buf_t bi;
        if (ai_json_buf_init(&bi)) return -1;

        char ts[64];
        get_iso_timestamp(ts, sizeof(ts));
        ai_json_buf_append(&bi, "{\n");
        ai_json_buf_appendf(&bi, "\"version\":\"1.0\",\n");
        ai_json_buf_appendf(&bi, "\"generated_at\":");
        ai_json_escape(ts, &bi);
        ai_json_buf_appendf(&bi, ",\n\"business_name\":");
        ai_json_escape(config->business_name ? config->business_name : "default", &bi);
        ai_json_buf_append(&bi, ",\n\"symbols_by_entity\":{\n");
        /* Empty entity set for now — populated by bs_biz_introspect offline */
        ai_json_buf_append(&bi, "  }\n}\n");

        snprintf(path, sizeof(path), "%s/biz_semantic_index.json", config->output_dir);
        int ret = write_file(path, bi.buf);
        ai_json_buf_destroy(&bi);
        if (ret) return ret;

        /* Write fingerprint */
        snprintf(path, sizeof(path), "%s/biz_index_fingerprint.txt", config->output_dir);
        FILE* fp = fopen(path, "w");
        if (fp) {
            fprintf(fp, "biz_semantic_index:v1:%s:%s\n",
                    ts, config->business_name ? config->business_name : "default");
            fclose(fp);
        }
    }

    return 0;
}

int bs_agent_index_export_schema_only(
    bs_schema_registry_t*  reg,
    const bs_agent_index_config_t* config)
{
    return bs_agent_index_export(reg, NULL, config);
}
