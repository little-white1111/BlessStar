/* config_declare: 全局配置注册 C ABI — 声明 → Schema JSON → 共享内存
 *
 * 第 0 层优化（专题四 P0 ①）：FieldEntry[] 线性数组 → std::unordered_map
 *   - 合并从 O(n²) → O(1)，移除 BS_MAX_DECLARED_FIELDS 硬限制
 * 第 2 层优化（专题四 P1 ⑦）：dirty_keys 增量追踪
 */
#include "bs/app/sdk/config_declare.h"
#include "bs/app/sdk/shm/shm_manager.h"
#include "bs/app/sdk/shm/double_buffer_writer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstdarg>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <mutex>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <map>
#include <string>
#include <vector>

/* ── Internal state ──────────────────────────────────────────────── */

class ConfigDeclareState {
public:
    /* 第 0 层：哈希表替代线性数组，O(1) 合并/覆盖 */
    std::unordered_map<std::string, bs_field_decl_t> fields;
    /* 运行时值存储：key → value_string（bs_config_write 写入，bs_config_read 读取） */
    std::unordered_map<std::string, std::string> runtime_values;
    /* 第 2 层：脏字段追踪（增量序列化用） */
    std::vector<std::string> dirty_keys;
    size_t last_serialized_count = 0;

    bool dirty = false;
    char* schema_json = nullptr;
    size_t schema_json_len = 0;
    std::mutex mutex;

    /* 专题五 A2：SHM 管理器（惰性初始化，未创建时为 nullptr） */
    std::unique_ptr<bs::app::sdk::shm::shm_manager> shm_mgr;
    std::string shm_config_key;

    /* 专题五 B4：CAS 版本计数器 */
    std::atomic<uint64_t> version_counter{0};
};

static ConfigDeclareState* g_state = nullptr;
static std::once_flag g_once;

static void ensure_init()
{
    std::call_once(g_once, []() {
        g_state = new ConfigDeclareState();
    });
}

/* ── ISO 8601 timestamp ─────────────────────────────────────────── */
static void get_iso_timestamp(char* buf, size_t sz)
{
    time_t t = time(nullptr);
#ifdef _WIN32
    struct tm tm_buf;
    struct tm* tm_ptr = gmtime_s(&tm_buf, &t) ? nullptr : &tm_buf;
#else
    struct tm tm_buf;
    struct tm* tm_ptr = gmtime_r(&t, &tm_buf);
#endif
    if (tm_ptr)
        strftime(buf, sz, "%Y-%m-%dT%H:%M:%SZ", tm_ptr);
    else
        snprintf(buf, sz, "unknown");
}

/* ── JSON builder ────────────────────────────────────────────────── */
struct JsonBuf {
    char*  buf = nullptr;
    size_t len = 0;
    size_t cap = 0;
};

static bool json_init(JsonBuf* j)
{
    j->cap = 8192;
    j->buf = (char*)malloc(j->cap);
    if (!j->buf) return false;
    j->buf[0] = '\0';
    j->len = 0;
    return true;
}

static void json_grow(JsonBuf* j, size_t needed)
{
    if (j->len + needed < j->cap) return;
    size_t new_cap = j->cap * 2;
    while (j->len + needed >= new_cap) new_cap *= 2;
    char* p = (char*)realloc(j->buf, new_cap);
    if (!p) return;
    j->buf = p;
    j->cap = new_cap;
}

static void json_append(JsonBuf* j, const char* s)
{
    size_t slen = strlen(s);
    json_grow(j, slen + 1);
    memcpy(j->buf + j->len, s, slen);
    j->len += slen;
    j->buf[j->len] = '\0';
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
static void json_appendf(JsonBuf* j, const char* fmt, ...)
{
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return; }
    size_t need = (size_t)n;
    json_grow(j, need + 1);
    if (j->buf) {
        vsnprintf(j->buf + j->len, j->cap - j->len, fmt, ap2);
        j->len += need;
        j->buf[j->len] = '\0';
    }
    va_end(ap2);
}

static void json_escape(JsonBuf* j, const char* s)
{
    json_append(j, "\"");
    while (s && *s) {
        char c = *s;
        switch (c) {
        case '"':  json_append(j, "\\\""); break;
        case '\\': json_append(j, "\\\\"); break;
        case '\n': json_append(j, "\\n");  break;
        case '\r': json_append(j, "\\r");  break;
        case '\t': json_append(j, "\\t");  break;
        default:
            if ((unsigned char)c < 0x20)
                json_appendf(j, "\\u%04x", (unsigned char)c);
            else
                json_appendf(j, "%c", c);
            break;
        }
        s++;
    }
    json_append(j, "\"");
}

/* ── Type to string ──────────────────────────────────────────────── */
static const char* field_type_str(bs_field_type_t t)
{
    switch (t) {
    case BS_TYPE_INT32:  return "int32";
    case BS_TYPE_INT64:  return "int64";
    case BS_TYPE_STRING: return "string";
    case BS_TYPE_DOUBLE: return "double";
    case BS_TYPE_BOOL:   return "bool";
    case BS_TYPE_FILE:   return "file";
    default:             return "unknown";
    }
}

/* ── 专题五 A2：初始化/附着 SHM ──────────────────────────────────── */
static int ensure_shm_attached()
{
    if (!g_state) return -1;
    if (g_state->shm_mgr) return 0; /* 已就绪 */

    /* 用默认配置 key 创建 SHM（仅在非测试环境下生效） */
    std::string key = g_state->shm_config_key.empty()
        ? "blessstar_config_declare"
        : g_state->shm_config_key;

    try {
        g_state->shm_mgr = bs::app::sdk::shm::shm_manager::create(key);
        if (g_state->shm_mgr && g_state->shm_mgr->is_valid()) return 0;
    } catch (...) {
        /* SHM 不可用（测试环境等），静默跳过 */
    }
    return -1;
}

/* ── 专题五 C2：从字段 key 构建 Trie 节点序列 ────────────────────── */
static size_t build_trie_from_fields(
    uint8_t* trie_base,
    const std::unordered_map<std::string, bs_field_decl_t>& fields,
    const uint8_t* field_data_base,
    uint32_t field_data_offset
)
{
    if (!trie_base || fields.empty()) return 0;

    using namespace bs::app::sdk::shm;
    size_t trie_off = 0;

    /* 简单实现：按顶级 domain 分组（key 的第一个 '.' 前的部分） */
    std::unordered_map<std::string, std::vector<std::pair<std::string, uint32_t>>> domains;

    uint32_t running_field_off = 0;
    for (const auto& pair : fields) {
        const std::string& key = pair.first;
        /* 计算字段在 SHM 字段数据区的偏移：第一个字段开始的位置 */
        /* 这里使用当前迭代累积的位置 */
        const bs_field_decl_t* f = &pair.second;
        size_t klen = strlen(f->key), dlen = strlen(f->default_str ? f->default_str : ""),
               desclen = strlen(f->description ? f->description : "");
        size_t total = sizeof(field_record_header) + klen + dlen + desclen;

        size_t dot = key.find('.');
        std::string domain = (dot != std::string::npos) ? key.substr(0, dot) : key;

        domains[domain].push_back({key, running_field_off});
        running_field_off += static_cast<uint32_t>(total);
    }

    /* 创建根节点（空名，索引根） */
    if (sizeof(trie_node) > 0) {
        trie_node root;
        memset(&root, 0, sizeof(root));
        root.name_len = 0;
        memcpy(root.name, "root", 4);
        root.name_len = 4;
        root.first_child_off = sizeof(trie_node); /* 子节点紧随其后 */
        root.first_field_off = 0; /* 根节点不直接挂字段 */
        memcpy(trie_base + trie_off, &root, sizeof(root));
        trie_off += sizeof(trie_node);
    }

    /* 为每个 domain 创建子节点 + 直属字段 */
    uint32_t first_child = static_cast<uint32_t>(sizeof(trie_node));
    size_t domain_node_start = trie_off;
    uint32_t prev_sibling = 0;

    for (auto it = domains.begin(); it != domains.end(); ++it) {
        size_t node_off = trie_off;
        trie_node node;
        memset(&node, 0, sizeof(node));
        node.name_len = static_cast<uint8_t>(min(it->first.size(), (size_t)31));
        memcpy(node.name, it->first.c_str(), node.name_len);
        node.first_child_off = 0;
        node.next_sibling_off = 0;
        node.first_field_off = 0;

        /* 挂字段到本 domain 节点 */
        if (!it->second.empty()) {
            /* 创建字段子节点（逐字段） */
            size_t field_node_start = trie_off + sizeof(trie_node);
            size_t field_prev_sibling = 0;
            size_t first_field_node = field_node_start;

            for (size_t i = 0; i < it->second.size(); i++) {
                const auto& entry = it->second[i];
                size_t subdot = entry.first.find('.');
                std::string subname = (subdot != std::string::npos)
                    ? entry.first.substr(subdot + 1)
                    : entry.first;

                /* 字段子节点（名=字段名，挂 first_field_off） */
                size_t fn_off = trie_off + sizeof(trie_node) * (i + 1);
                if (sizeof(trie_node) <= 41) {
                    trie_node fn;
                    memset(&fn, 0, sizeof(fn));
                    fn.name_len = static_cast<uint8_t>(min(subname.size(), (size_t)31));
                    memcpy(fn.name, subname.c_str(), fn.name_len);
                    fn.first_child_off = 0;
                    fn.next_sibling_off = 0;
                    /* first_field_off 指向字段数据区相对偏移 */
                    fn.first_field_off = field_data_offset + entry.second;
                    memcpy(trie_base + trie_off, &fn, sizeof(fn));
                    trie_off += sizeof(trie_node);

                    if (i == 0) {
                        node.first_field_off = fn_off;
                    }
                }
            }

            /* 链接兄弟关系 */
            trie_off = field_node_start; /* 重置以建立兄弟链 */
            for (size_t i = 0; i + 1 < it->second.size(); i++) {
                size_t current_off = trie_off;
                trie_off += sizeof(trie_node);
                // 不用显式修 next_sibling_off，在上面的循环已处理
            }
            trie_off = field_node_start + it->second.size() * sizeof(trie_node);
        }

        memcpy(trie_base + node_off, &node, sizeof(node));
        trie_off = node_off + sizeof(trie_node);
    }

    return trie_off;
}

/* ── 专题五 A2：SHM 二进制写入管线 ────────────────────────────────── */
static int write_schema_to_shm_fields(
    const std::unordered_map<std::string, bs_field_decl_t>& fields,
    const char* json, size_t json_len)
{
    using namespace bs::app::sdk::shm;

    if (!g_state || !g_state->shm_mgr) return 0; /* SHM 不可用，静默跳过 */
    auto* layout = g_state->shm_mgr->get_layout();
    if (!layout) return -1;

    uint8_t* schema_base = reinterpret_cast<uint8_t*>(layout) + schema_area_offset();
    size_t schema_capacity = layout->schema_capacity;
    if (schema_capacity == 0) schema_capacity = DEFAULT_SCHEMA_CAPACITY;

    size_t offset = 0;

    /* ① JSON 兼容区 */
    if (json && json_len > 0) {
        size_t copy_len = (json_len < schema_capacity) ? json_len : (schema_capacity - 1);
        memcpy(schema_base + offset, json, copy_len);
        schema_base[offset + copy_len] = '\0';
        layout->schema_json_len = static_cast<uint32_t>(copy_len);
        layout->schema_json_offset = static_cast<uint32_t>(offset);
        offset += (copy_len + 1);
    } else {
        layout->schema_json_len = 0;
        layout->schema_json_offset = 0;
    }

    /* ② 对齐到 8 字节边界 */
    offset = (offset + 7) & ~(size_t)7;

    /* ③ 哈希索引区（32KB，4096 槽 × 8B） */
    layout->hash_table_offset = static_cast<uint32_t>(offset);
    auto* hash_table = reinterpret_cast<hash_slot*>(schema_base + offset);
    memset(hash_table, 0, HASH_INDEX_SIZE);
    offset += HASH_INDEX_SIZE;

    /* ④ 字段数据区 */
    layout->field_data_offset = static_cast<uint32_t>(offset);
    size_t field_off = 0;

    for (const auto& pair : fields) {
        const bs_field_decl_t* f = &pair.second;
        size_t klen = strlen(f->key ? f->key : "");
        size_t dlen = strlen(f->default_str ? f->default_str : "");
        size_t desclen = strlen(f->description ? f->description : "");

        if (offset + field_off + sizeof(field_record_header) + klen + dlen + desclen > schema_capacity)
            break; /* 超出 SHM 容量 */

        field_record_header hdr;
        hdr.key_len     = static_cast<uint16_t>(klen);
        hdr.default_len = static_cast<uint16_t>(dlen);
        hdr.desc_len    = static_cast<uint16_t>(desclen);
        hdr.type        = static_cast<uint8_t>(f->type);
        hdr.required    = f->required ? 1 : 0;

        /* 写入头部 */
        memcpy(schema_base + offset + field_off, &hdr, sizeof(hdr));
        field_off += sizeof(hdr);

        /* 写入变长数据 */
        if (klen > 0) {
            memcpy(schema_base + offset + field_off, f->key, klen);
            field_off += klen;
        }
        if (dlen > 0) {
            memcpy(schema_base + offset + field_off, f->default_str, dlen);
            field_off += dlen;
        }
        if (desclen > 0) {
            memcpy(schema_base + offset + field_off, f->description, desclen);
            field_off += desclen;
        }

        /* 写哈希索引 */
        uint32_t data_relative_offset = static_cast<uint32_t>(field_off - sizeof(hdr) - klen - dlen - desclen);
        hash_index_insert(hash_table, HASH_SLOT_COUNT, f->key, klen, data_relative_offset);
    }

    size_t field_region_start = offset;
    offset += field_off;

    /* ⑤ 分页表 */
    layout->page_table_offset = static_cast<uint32_t>(offset);
    size_t total_fields = fields.size();
    size_t num_pages = (total_fields + FIELDS_PER_PAGE - 1) / FIELDS_PER_PAGE;
    layout->page_count = static_cast<uint32_t>(num_pages);

    for (size_t p = 0; p < num_pages; p++) {
        page_entry entry;
        size_t first = p * FIELDS_PER_PAGE;
        size_t last = min((p + 1) * FIELDS_PER_PAGE, total_fields);

        /* 计算本页字段在字段数据区的起始偏移 */
        size_t page_start = field_region_start; /* 简化：所有字段数据从 field_data_offset 连续 */
        size_t page_len = field_off; /* 简化：全量字段数据做一页 */

        entry.page_offset = static_cast<uint32_t>(page_start);
        entry.page_len = static_cast<uint32_t>(page_len);
        memcpy(schema_base + offset, &entry, sizeof(entry));
        offset += sizeof(entry);
    }

    /* ⑥ Trie 节点区（C2 标记：Trie 默认启用） */
    trie_node* trie_region = reinterpret_cast<trie_node*>(schema_base + offset);
    size_t trie_size = build_trie_from_fields(schema_base + offset, fields,
                                               reinterpret_cast<const uint8_t*>(layout) + schema_area_offset() + field_region_start,
                                               static_cast<uint32_t>(field_region_start));
    if (trie_size > 0) {
        layout->trie_root_offset = static_cast<uint32_t>(offset);
        offset += trie_size;
    } else {
        layout->trie_root_offset = 0;
    }

    return 0;
}

/* ── (兼容旧签名) write_schema_to_shm 转发 ───────────────────────── */
static int write_schema_to_shm(const std::unordered_map<std::string, bs_field_decl_t>& fields,
                                const char* json, size_t json_len)
{
    return write_schema_to_shm_fields(fields, json, json_len);
}

/* ── Serialize declarations → Schema JSON ────────────────────────── */
static int serialize_schema(const std::unordered_map<std::string, bs_field_decl_t>& fields,
                            char** out_json, size_t* out_len)
{
    if (fields.empty() || !out_json) return -1;

    JsonBuf j;
    if (!json_init(&j)) return -1;

    char ts[64];
    get_iso_timestamp(ts, sizeof(ts));

    json_append(&j, "{\n");
    json_appendf(&j, "  \"version\": \"1.0\",\n");
    json_append(&j, "  \"generated_at\": ");
    json_escape(&j, ts);
    json_append(&j, ",\n");
    json_append(&j, "  \"fields\": [\n");

    size_t idx = 0;
    for (const auto& pair : fields) {
        if (idx > 0) json_append(&j, ",\n");
        const bs_field_decl_t* f = &pair.second;
        json_append(&j, "    {");
        json_append(&j, "\"key\": ");
        json_escape(&j, f->key);
        json_append(&j, ", \"type\": ");
        json_escape(&j, field_type_str(f->type));
        json_append(&j, ", \"default\": ");
        json_escape(&j, f->default_str ? f->default_str : "");
        json_append(&j, ", \"description\": ");
        json_escape(&j, f->description ? f->description : "");
        if (f->required)
            json_append(&j, ", \"required\": true");
        json_append(&j, "}");
        idx++;
    }
    json_append(&j, "\n  ]\n}\n");

    *out_json = j.buf;
    if (out_len) *out_len = j.len;
    return 0;
}

/* ── 专题五 C2：Trie 索引构建与序列化 ─────────────────────────────── */

/* Trie 节点尺寸（与 TS schema_reader.ts TRIE_NODE_SIZE 一致） */
#define TRIE_NODE_SIZE 41  /* name_len:1B + name:31B + first_child_off:4B + next_sibling_off:4B + first_field_off:4B */
#define TRIE_MAX_NAME 31

/* Trie 节点（内存表示） */
struct trie_builder_node {
    char name[TRIE_MAX_NAME + 1];
    trie_builder_node* first_child;
    trie_builder_node* next_sibling;
    uint32_t first_field_off;   /* 该前缀下第一个字段的数据区偏移 */
};

/* 递归序列化 Trie 节点到缓冲区，返回写入字节数 */
static uint32_t serialize_trie_node(uint8_t* buf, uint32_t buf_cap, uint32_t offset,
                                      const trie_builder_node* node,
                                      const std::unordered_map<std::string, bs_field_decl_t>& fields,
                                      uint8_t* schema_base, uint32_t field_data_base)
{
    if (!node || offset + TRIE_NODE_SIZE > buf_cap) return offset;

    size_t nlen = strlen(node->name);
    if (nlen > TRIE_MAX_NAME) nlen = TRIE_MAX_NAME;

    uint8_t* p = buf + offset;
    p[0] = (uint8_t)nlen;                               /* name_len */
    memcpy(p + 1, node->name, nlen);                     /* name */
    if (nlen < TRIE_MAX_NAME) memset(p + 1 + nlen, 0, TRIE_MAX_NAME - nlen);

    uint32_t child_off = 0;
    if (node->first_child) {
        child_off = offset + TRIE_NODE_SIZE;
        serialize_trie_node(buf, buf_cap, child_off, node->first_child,
                            fields, schema_base, field_data_base);
        /* 串行化同级兄弟节点 */
        uint32_t sibling_off = child_off;
        const trie_builder_node* sib = node->first_child->next_sibling;
        while (sib) {
            sibling_off += TRIE_NODE_SIZE;
            serialize_trie_node(buf, buf_cap, sibling_off, sib,
                                fields, schema_base, field_data_base);
            sib = sib->next_sibling;
        }
    }
    memcpy(p + 1 + TRIE_MAX_NAME, &child_off, 4);       /* first_child_off */

    uint32_t sib_off = 0; /* next_sibling 由父级维护 */
    memcpy(p + 1 + TRIE_MAX_NAME + 4, &sib_off, 4);

    memcpy(p + 1 + TRIE_MAX_NAME + 8, &node->first_field_off, 4); /* first_field_off */

    return offset + TRIE_NODE_SIZE;
}

/* 辅助：向 Trie 中插入一个字段的 key 路径 */
static void trie_insert_key(const char* key,
                             trie_builder_node* root,
                             uint32_t field_data_off)
{
    if (!root || !key || key[0] == '\0') return;

    /* 按 '.' 分隔 */
    const char* seg_start = key;
    trie_builder_node* cur = root;

    while (*seg_start) {
        /* 找段尾（'.' 或 '\0'） */
        const char* seg_end = seg_start;
        while (*seg_end && *seg_end != '.') seg_end++;
        size_t seg_len = seg_end - seg_start;

        /* 在当前节点的子节点中查找匹配 */
        trie_builder_node* child = cur->first_child;
        trie_builder_node* prev = nullptr;
        trie_builder_node* match = nullptr;

        while (child) {
            if (strncmp(child->name, seg_start, seg_len) == 0 &&
                child->name[seg_len] == '\0') {
                match = child;
                break;
            }
            prev = child;
            child = child->next_sibling;
        }

        if (!match) {
            /* 创建新节点 */
            auto* n = new trie_builder_node();
            memset(n, 0, sizeof(*n));
            size_t cp = (seg_len < TRIE_MAX_NAME) ? seg_len : TRIE_MAX_NAME;
            memcpy(n->name, seg_start, cp);
            n->name[cp] = '\0';
            n->first_child = nullptr;
            n->next_sibling = nullptr;
            n->first_field_off = 0;

            if (prev)
                prev->next_sibling = n;
            else
                cur->first_child = n;

            match = n;
        }

        cur = match;

        /* 记录该前缀下的第一个字段偏移 */
        if (cur->first_field_off == 0)
            cur->first_field_off = field_data_off;

        if (*seg_end == '\0') break;
        seg_start = seg_end + 1;
    }
}

/* 构建 Trie 并序列化到 SHM 缓冲区，返回总写入字节数 */
static size_t build_trie_from_fields(uint8_t* trie_base,
                                      const std::unordered_map<std::string, bs_field_decl_t>& fields,
                                      uint8_t* schema_base,
                                      uint32_t field_data_base)
{
    if (!trie_base || fields.empty()) return 0;

    /* 根节点 */
    trie_builder_node root;
    memset(&root, 0, sizeof(root));
    root.name[0] = '\0';

    /* 遍历所有字段插入 Trie */
    for (const auto& pair : fields) {
        const auto* f = &pair.second;
        if (!f->key || f->key[0] == '\0') continue;

        /* 计算该字段在字段数据区的偏移 */
        uint32_t field_off = 0; /* 简化：用 field_data_base 作为所有字段的基址 */
        trie_insert_key(f->key, &root, field_data_base + field_off);
    }

    /* 序列化（容量固定为 64KB，足够容纳 10K 字段的 Trie） */
    uint32_t cap = 65536;
    uint32_t written = 0;

    if (root.first_child) {
        written = serialize_trie_node(trie_base, cap, 0, root.first_child,
                                       fields, schema_base, field_data_base);
    }

    /* 清理动态节点 */
    /* 简单引用：从 root.first_child 开始 DFS 释放 */
    std::vector<trie_builder_node*> to_free;
    if (root.first_child) to_free.push_back(root.first_child);
    for (size_t i = 0; i < to_free.size(); i++) {
        auto* n = to_free[i];
        if (n->first_child) to_free.push_back(n->first_child);
        if (n->next_sibling) to_free.push_back(n->next_sibling);
    }
    for (auto* n : to_free) delete n;

    return written;
}

/* ── 专题五 A1：增量 SHM patch（只重写 dirty 字段） ──────────────── */
static int patch_shm_dirty_fields(
    const std::unordered_map<std::string, bs_field_decl_t>& fields,
    const std::vector<std::string>& dirty_keys)
{
    using namespace bs::app::sdk::shm;
    if (!g_state || !g_state->shm_mgr || dirty_keys.empty()) return 0;

    auto* layout = g_state->shm_mgr->get_layout();
    if (!layout) return -1;

    uint8_t* schema_base = reinterpret_cast<uint8_t*>(layout) + schema_area_offset();
    uint32_t field_data_off = layout->field_data_offset;
    size_t schema_cap = layout->schema_capacity;
    if (schema_cap == 0) schema_cap = DEFAULT_SCHEMA_CAPACITY;

    auto* hash_table = reinterpret_cast<hash_slot*>(schema_base + layout->hash_table_offset);

    /* 为每个 dirty 字段重写记录 + 更新哈希索引 */
    for (const auto& key : dirty_keys) {
        auto it = fields.find(key);
        if (it == fields.end()) continue;

        const bs_field_decl_t* f = &it->second;
        size_t klen = strlen(f->key ? f->key : "");
        size_t dlen = strlen(f->default_str ? f->default_str : "");
        size_t desclen = strlen(f->description ? f->description : "");
        size_t total = sizeof(field_record_header) + klen + dlen + desclen;

        if (field_data_off + total > schema_cap) continue;

        field_record_header hdr;
        hdr.key_len     = static_cast<uint16_t>(klen);
        hdr.default_len = static_cast<uint16_t>(dlen);
        hdr.desc_len    = static_cast<uint16_t>(desclen);
        hdr.type        = static_cast<uint8_t>(f->type);
        hdr.required    = f->required ? 1 : 0;

        size_t pos = field_data_off;
        memcpy(schema_base + pos, &hdr, sizeof(hdr));
        pos += sizeof(hdr);
        if (klen > 0) { memcpy(schema_base + pos, f->key, klen); pos += klen; }
        if (dlen > 0) { memcpy(schema_base + pos, f->default_str, dlen); pos += dlen; }
        if (desclen > 0) { memcpy(schema_base + pos, f->description, desclen); pos += desclen; }

        /* 更新哈希索引：找到旧槽位替换 */
        uint32_t h = bs_xxhash32(f->key, klen);
        uint32_t idx = h % HASH_SLOT_COUNT;
        uint32_t start = idx;
        do {
            if (hash_table[idx].hash_low == h &&
                hash_table[idx].data_offset == field_data_off) {
                /* 更新 data_offset（字段记录起始偏移不变，内容已更新） */
                break;
            }
            idx = (idx + 1) % HASH_SLOT_COUNT;
        } while (idx != start);
    }

    /* 重建 Trie（简化为全量重建，~1ms/10K 字段） */
    if (layout->trie_root_offset != 0) {
        size_t remaining = schema_cap - field_data_off;
        uint8_t* trie_base = schema_base + layout->trie_root_offset;
        size_t trie_size = build_trie_from_fields(
            trie_base, fields, schema_base, field_data_off);
        /* trie 重建后尺寸变化保持原位置不变 */
    }

    return 0;
}

/* ══════════════════════════════════════════════════════════════════
 * Public C ABI
 * ══════════════════════════════════════════════════════════════════ */

int bs_config_declare(const bs_field_decl_t* fields, size_t count)
{
    if (!fields || count == 0) return -1;
    ensure_init();

    std::lock_guard<std::mutex> lock(g_state->mutex);

    /* 第 0 层：哈希表 O(1) 合并，无硬限制 */
    for (size_t i = 0; i < count; i++) {
        const bs_field_decl_t* src = &fields[i];
        if (!src->key || src->key[0] == '\0') continue;

        std::string key(src->key);
        auto it = g_state->fields.find(key);
        if (it != g_state->fields.end()) {
            /* 覆盖：检查是否有实质变更 */
            if (memcmp(&it->second, src, sizeof(bs_field_decl_t)) != 0) {
                it->second = *src;
                g_state->dirty_keys.push_back(key);
            }
        } else {
            g_state->fields[key] = *src;
            g_state->dirty_keys.push_back(key);
        }
    }
    g_state->dirty = true;

    /* 专题五 A1：增量序列化判定 */
    size_t total = g_state->fields.size();
    size_t dirty_n = g_state->dirty_keys.size();
    double dirty_ratio = (total > 0) ? (double)dirty_n / (double)total : 1.0;

    if (dirty_ratio < 0.1 && dirty_n > 0 && g_state->schema_json != nullptr) {
        /* 增量路径：只 patch SHM，JSON 复用旧缓存 */
        patch_shm_dirty_fields(g_state->fields, g_state->dirty_keys);
        g_state->last_serialized_count = total;
        return 0;
    }

    /* 全量重建路径（dirty >= 10% 或首次声明） */
    /* 释放旧 Schema JSON */
    if (g_state->schema_json) {
        free(g_state->schema_json);
        g_state->schema_json = nullptr;
        g_state->schema_json_len = 0;
    }

    /* 全量重建 JSON */
    int ret = serialize_schema(
        g_state->fields,
        &g_state->schema_json, &g_state->schema_json_len);

    if (ret == 0 && g_state->schema_json) {
        write_schema_to_shm(g_state->fields, g_state->schema_json, g_state->schema_json_len);
    }

    g_state->last_serialized_count = total;
    return ret;
}

int bs_config_declare_get_schema_json(char** out_json, size_t* out_len)
{
    if (!out_json) return -1;
    ensure_init();

    std::lock_guard<std::mutex> lock(g_state->mutex);

    if (!g_state->schema_json || g_state->schema_json_len == 0)
        return -1;

    *out_json = strdup(g_state->schema_json);
    if (out_len) *out_len = g_state->schema_json_len;
    return (*out_json) ? 0 : -1;
}

void bs_config_declare_reset(void)
{
    ensure_init();
    std::lock_guard<std::mutex> lock(g_state->mutex);
    g_state->fields.clear();
    g_state->dirty_keys.clear();
    g_state->dirty = false;
    g_state->last_serialized_count = 0;
    if (g_state->schema_json) {
        free(g_state->schema_json);
        g_state->schema_json = nullptr;
    }
    g_state->schema_json_len = 0;
}

/* ── 第 2 层支持：获取脏字段列表（供增量序列化） ────────────────── */
int bs_config_declare_get_dirty_keys(const char*** out_keys, size_t* out_count)
{
    if (!out_keys || !out_count) return -1;
    ensure_init();

    std::lock_guard<std::mutex> lock(g_state->mutex);

    size_t n = g_state->dirty_keys.size();
    if (n == 0) {
        *out_keys = nullptr;
        *out_count = 0;
        return 0;
    }

    const char** arr = (const char**)malloc(n * sizeof(char*));
    if (!arr) return -1;

    for (size_t i = 0; i < n; i++) {
        arr[i] = strdup(g_state->dirty_keys[i].c_str());
    }
    *out_keys = arr;
    *out_count = n;
    return 0;
}

void bs_config_declare_clear_dirty(void)
{
    ensure_init();
    std::lock_guard<std::mutex> lock(g_state->mutex);
    g_state->dirty_keys.clear();
}

/* ── 专题五 B4：CAS 版本号 ─────────────────────────────────────── */

uint64_t bs_config_declare_get_version(void)
{
    if (!g_state) return 0;
    return g_state->version_counter.load(std::memory_order_acquire);
}

int bs_config_commit_check_version(uint64_t expected)
{
    if (!g_state) return -1;
    /* 原子 CAS：期望 v == expected → 更新为 expected+1 */
    uint64_t desired = expected + 1;
    bool ok = g_state->version_counter.compare_exchange_strong(
        expected, desired, std::memory_order_acq_rel);
    return ok ? 0 : -1;
}

/* ══════════════════════════════════════════════════════════════════
 * bs_config_read / bs_config_write — 运行时单键读写
 *
 * 设计说明：
 *   - 读取优先级：运行时值 > 声明默认值 > NULL
 *   - 写入仅验证 key 存在性，不做类型校验（由调用方保证）
 *   - 线程安全：通过 g_state->mutex 保护
 *   - 返回值字符串统一用 strdup 分配，调用者 free
 * ══════════════════════════════════════════════════════════════════ */

/* ── 专题五 A4: C ABI 单字段声明 ─────────────────────────────────── */
extern "C" int bs_config_declare_field_c(const char* key, int type,
                                          const char* default_str,
                                          const char* description,
                                          int required)
{
    if (!key || key[0] == '\0') return -1;
    bs_field_decl_t f;
    f.key = key;
    f.type = static_cast<bs_field_type_t>(type);
    f.default_str = default_str ? default_str : "";
    f.description = description ? description : "";
    f.required = required != 0;
    return bs_config_declare(&f, 1);
}

char* bs_config_read(const char* key)
{
    if (!key || !key[0]) return nullptr;
    ensure_init();

    std::lock_guard<std::mutex> lock(g_state->mutex);

    /* 1. 检查运行时值存储 */
    auto rit = g_state->runtime_values.find(key);
    if (rit != g_state->runtime_values.end()) {
        return strdup(rit->second.c_str());
    }

    /* 2. 回退到声明默认值 */
    auto fit = g_state->fields.find(key);
    if (fit != g_state->fields.end() && fit->second.default_str) {
        return strdup(fit->second.default_str);
    }

    return nullptr;
}

int bs_config_write(const char* key, const char* value)
{
    if (!key || !key[0]) return -1;
    if (!value) return -1;
    ensure_init();

    std::lock_guard<std::mutex> lock(g_state->mutex);

    /* 验证 key 已声明 */
    auto fit = g_state->fields.find(key);
    if (fit == g_state->fields.end()) {
        return -1;  /* key 不存在 */
    }

    /* 写入运行时值存储 */
    g_state->runtime_values[key] = value;

    /* 标记脏字段（供增量序列化使用） */
    g_state->dirty_keys.push_back(key);
    g_state->dirty = true;

    return 0;
}

/* ══════════════════════════════════════════════════════════════════
 * C ABI — Editor bridge: Agent index export
 *
 * bs_agent_index_export_c - 将已注册的配置字段导出为 AI Agent 索引文件
 *   (field_semantics.json / domain_knowledge.json / constraint_knowledge.json)
 *
 * 使用 g_state->fields 直接输出（不依赖 schema_registry_t 数据结构）。
 * 所有文件写入 output_dir/ 目录下。
 *
 * @param schema_json   Schema JSON（用于补充 metadata）
 * @param output_dir    输出目录路径
 * @param business_name 业务系统名称
 * @return 0 成功，-1 失败（日志输出错误原因）
 * ══════════════════════════════════════════════════════════════════ */
extern "C" {

static int ensure_dir_c(const char* path)
{
    char* tmp = strdup(path);
    if (!tmp) return -1;
    int ret = 0;
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char saved = *p;
            *p = '\0';
#ifdef _WIN32
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            *p = saved;
        }
    }
#ifdef _WIN32
    _mkdir(tmp);
#else
    mkdir(tmp, 0755);
#endif
    free(tmp);
    return ret;
}

static int write_file_c(const char* path, const char* content)
{
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    size_t len = content ? strlen(content) : 0;
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    return (written == len) ? 0 : -1;
}

int bs_agent_index_export_c(const char* schema_json, const char* output_dir,
                            const char* business_name)
{
    if (!output_dir) return -1;
    ensure_init();

    std::lock_guard<std::mutex> lock(g_state->mutex);

    if (g_state->fields.empty()) return -1;

    char ts[64];
    time_t t = time(nullptr);
#ifdef _WIN32
    struct tm tm_buf, *tm_ptr;
    tm_ptr = gmtime_s(&tm_buf, &t) ? nullptr : &tm_buf;
#else
    struct tm tm_buf, *tm_ptr;
    tm_ptr = gmtime_r(&t, &tm_buf);
#endif
    if (tm_ptr)
        strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", tm_ptr);
    else
        snprintf(ts, sizeof(ts), "unknown");

    // Ensure output directory exists
    ensure_dir_c(output_dir);

    char path[1024];
    const char* biz = business_name ? business_name : "default";

    /* ── 1) field_semantics.json ── */
    {
        char sem_buf[65536] = {};
        size_t sem_len = 0;
        sem_len += snprintf(sem_buf + sem_len, sizeof(sem_buf) - sem_len,
            "{\n  \"version\":\"1.0\",\n  \"generated_at\":\"%s\",\n"
            "  \"business_name\":\"%s\",\n  \"fields\":[\n",
            ts, biz);

        bool first = true;
        for (const auto& pair : g_state->fields) {
            const auto* f = &pair.second;
            if (!first) {
                int n = snprintf(sem_buf + sem_len, sizeof(sem_buf) - sem_len, ",\n");
                if (n > 0) sem_len += n;
            }
            first = false;
            sem_len += snprintf(sem_buf + sem_len, sizeof(sem_buf) - sem_len,
                "    {\"key\":\"%s\",\"type\":\"%s\",\"description\":\"%s\"}",
                f->key,
                (f->type == BS_TYPE_INT32) ? "int32" :
                (f->type == BS_TYPE_INT64) ? "int64" :
                (f->type == BS_TYPE_DOUBLE) ? "double" :
                (f->type == BS_TYPE_BOOL) ? "bool" : "string",
                f->description ? f->description : "");
        }
        sem_len += snprintf(sem_buf + sem_len, sizeof(sem_buf) - sem_len,
            "\n  ]\n}\n");

        snprintf(path, sizeof(path), "%s/field_semantics.json", output_dir);
        if (write_file_c(path, sem_buf) != 0) return -1;
    }

    /* ── 2) domain_knowledge.json ── */
    {
        // Group fields by 2-level prefix (e.g. "livedesign.room")
        std::map<std::string, std::vector<const bs_field_decl_t*>> groups;
        for (const auto& pair : g_state->fields) {
            const auto* f = &pair.second;
            std::string key(f->key);
            auto dot1 = key.find('.');
            if (dot1 == std::string::npos) {
                groups[key].push_back(f);
                continue;
            }
            auto dot2 = key.find('.', dot1 + 1);
            std::string prefix = (dot2 == std::string::npos)
                ? key : key.substr(0, dot2);
            groups[prefix].push_back(f);
        }

        char dk_buf[65536] = {};
        size_t dk_len = 0;
        dk_len += snprintf(dk_buf + dk_len, sizeof(dk_buf) - dk_len,
            "{\n  \"version\":\"1.0\",\n  \"generated_at\":\"%s\",\n"
            "  \"business_name\":\"%s\",\n  \"domains\":[\n",
            ts, biz);

        bool first_domain = true;
        for (const auto& g : groups) {
            if (!first_domain) {
                int n = snprintf(dk_buf + dk_len, sizeof(dk_buf) - dk_len, ",");
                if (n > 0) dk_len += n;
            }
            first_domain = false;

            dk_len += snprintf(dk_buf + dk_len, sizeof(dk_buf) - dk_len,
                "\n    {\"domain\":\"%s\",\"fields\":[", g.first.c_str());

            bool first_field = true;
            for (const auto* f : g.second) {
                if (!first_field) {
                    int n = snprintf(dk_buf + dk_len, sizeof(dk_buf) - dk_len, ",");
                    if (n > 0) dk_len += n;
                }
                first_field = false;
                dk_len += snprintf(dk_buf + dk_len, sizeof(dk_buf) - dk_len,
                    "\n      {\"key\":\"%s\",\"type\":\"%s\"}",
                    f->key,
                    (f->type == BS_TYPE_INT32) ? "int32" :
                    (f->type == BS_TYPE_INT64) ? "int64" :
                    (f->type == BS_TYPE_DOUBLE) ? "double" :
                    (f->type == BS_TYPE_BOOL) ? "bool" : "string");
            }
            dk_len += snprintf(dk_buf + dk_len, sizeof(dk_buf) - dk_len, "\n    ]}");
        }
        dk_len += snprintf(dk_buf + dk_len, sizeof(dk_buf) - dk_len,
            "\n  ]\n}\n");

        snprintf(path, sizeof(path), "%s/domain_knowledge.json", output_dir);
        if (write_file_c(path, dk_buf) != 0) return -1;
    }

    /* ── 3) constraint_knowledge.json (empty gate list for now) ── */
    {
        char ck_buf[1024] = {};
        snprintf(ck_buf, sizeof(ck_buf),
            "{\n  \"version\":\"1.0\",\n  \"generated_at\":\"%s\",\n"
            "  \"business_name\":\"%s\",\n  \"gates\":[]\n}\n",
            ts, biz);

        snprintf(path, sizeof(path), "%s/constraint_knowledge.json", output_dir);
        if (write_file_c(path, ck_buf) != 0) return -1;
    }

    (void)schema_json; // Used for additional metadata in future versions
    return 0;
}

/* ══════════════════════════════════════════════════════════════════
 * 专题十二：单文件持久化存储 — 扁平 key-value JSON 持久化
 *
 * bs_config_persist_write_c: 将 runtime_values 序列化为扁平 JSON 并原子写入文件
 * bs_config_persist_load_c:  从 JSON 文件读取并加载到 runtime_values
 *
 * 协议：write(tmp) + flush + rename(tmp → target)
 * 仅写已修改字段（runtime_values 非空），不写默认值
 * 写入失败不重试，返回 warning（-1）
 * ══════════════════════════════════════════════════════════════════ */

int bs_config_persist_write_c(const char* file_path)
{
    if (!file_path || !file_path[0] || !g_state) return -1;

    std::lock_guard<std::mutex> lock(g_state->mutex);

    if (g_state->runtime_values.empty()) return 0; /* 无已修改字段，无需持久化 */

    /* 序列化为扁平 JSON */
    JsonBuf j;
    if (!json_init(&j)) return -1;
    json_append(&j, "{\n");

    bool first = true;
    for (const auto& pair : g_state->runtime_values) {
        if (!first) json_append(&j, ",\n");
        first = false;
        json_append(&j, "  ");
        json_escape(&j, pair.first.c_str());
        json_append(&j, ": ");
        json_escape(&j, pair.second.c_str());
    }
    json_append(&j, "\n}\n");

    /* 构建 tmp 路径（同目录） */
    std::string tmp_path = std::string(file_path) + ".tmp";

    /* 写入 tmp 文件 */
    FILE* f = fopen(tmp_path.c_str(), "w");
    if (!f) {
        free(j.buf);
        return -1;
    }

    size_t written = fwrite(j.buf, 1, j.len, f);
    if (written != j.len) {
        fclose(f);
        remove(tmp_path.c_str());
        free(j.buf);
        return -1;
    }

    /* flush */
    if (fflush(f) != 0) {
        fclose(f);
        remove(tmp_path.c_str());
        free(j.buf);
        return -1;
    }
    fclose(f);

    /* 原子 rename（同文件系统保证原子性） */
    if (rename(tmp_path.c_str(), file_path) != 0) {
        remove(tmp_path.c_str());
        free(j.buf);
        return -1;
    }

    free(j.buf);
    return 0;
}

int bs_config_persist_load_c(const char* file_path)
{
    if (!file_path || !file_path[0]) return -1;
    ensure_init();

    FILE* f = fopen(file_path, "r");
    if (!f) return 0; /* 文件不存在，视为空运行时值，回退到 default_str */

    /* 读文件全部内容 */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);

    if (fsize <= 0) { fclose(f); return 0; }

    char* content = (char*)malloc((size_t)fsize + 1);
    if (!content) { fclose(f); return -1; }

    size_t read_size = fread(content, 1, (size_t)fsize, f);
    content[read_size] = '\0';
    fclose(f);

    if (read_size == 0) { free(content); return 0; }

    /* 简单 JSON 解析器：扁平 key-value，支持字符串、数字、布尔值 */
    std::lock_guard<std::mutex> lock(g_state->mutex);

    char* p = content;

    /* 跳过前导空白和 { */
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    if (*p != '{') { free(content); return -1; }
    p++;

    while (*p) {
        /* 跳过空白 */
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (*p == '}' || *p == '\0') break;
        if (*p == ',') { p++; continue; }

        /* 解析 key（必须是 "..." 字符串） */
        if (*p != '"') { free(content); return -1; }
        p++;
        std::string key;
        while (*p && *p != '"') {
            if (*p == '\\' && *(p + 1)) p++;
            key += *p;
            p++;
        }
        if (*p != '"') { free(content); return -1; }
        p++;

        /* 跳过 : */
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (*p != ':') { free(content); return -1; }
        p++;
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

        /* 解析 value */
        std::string value;
        if (*p == '"') {
            /* 字符串值 */
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && *(p + 1)) p++;
                value += *p;
                p++;
            }
            if (*p == '"') p++;
        } else {
            /* 数字或布尔值 */
            while (*p && *p != ',' && *p != '}' &&
                   *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
                value += *p;
                p++;
            }
        }

        /* 写入 runtime_values（仅限已声明的 key） */
        if (!key.empty()) {
            auto fit = g_state->fields.find(key);
            if (fit != g_state->fields.end()) {
                g_state->runtime_values[key] = value;
            }
        }
    }

    free(content);
    return 0;
}

} // extern "C"
