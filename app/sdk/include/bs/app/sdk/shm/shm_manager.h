#pragma once

#include <atomic>
#include <cstddef>
#include <string>
#include <memory>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#endif

namespace bs {
namespace app {
namespace sdk {
namespace shm {

/*
 * SHM 布局（专题四 P0 ②：Schema 区从静态 64KB 扩容为可配置大小）
 *
 * +0x0000  version_counter       (8B)
 * +0x0008  active_buf_idx        (8B)
 * +0x0010  data_len              (8B)
 * +0x0018  schema_json_len       (4B)
 * +0x001c  schema_json_offset    (4B)
 * +0x0020  schema_capacity       (4B)  ← P0 ②：Schema 区容量（字节）
 * +0x0024  hash_table_offset     (4B)  ← P1 ⑤：哈希索引区偏移
 * +0x0028  field_data_offset     (4B)  ← P1 ⑤：字段数据区偏移
 * +0x002c  page_table_offset     (4B)  ← P2 ⑧：分页表偏移
 * +0x0030  page_count            (4B)  ← P2 ⑧：数据页总数
 * +0x0034  trie_root_offset      (4B)  ← P3 ⑨：Trie 根节点偏移
 * +0x0038  reserved[8]           (8B)
 * +0x0040  bufA[64KB]
 * +0x10040 bufB[64KB]
 * +0x20040 Schema 区（由 schema_capacity 决定大小）
 *          ├─ JSON 兼容区（首部，可变大小）
 *          ├─ 哈希索引区（P1 ⑤，4096 槽 × 8B = 32KB）
 *          ├─ 字段数据区（P1 ⑤，FlatBuffer 二进制）
 *          ├─ 分页表（P2 ⑧，每页 8B）
 *          └─ Trie 节点区（P3 ⑨，紧凑 trie_node）
 */
struct shm_config_layout {
    uint64_t version_counter;    // +0x00
    uint64_t active_buf_idx;     // +0x08
    uint64_t data_len;           // +0x10
    uint32_t schema_json_len;    // +0x18  Schema JSON 字节长度（0 = 无声明）
    uint32_t schema_json_offset; // +0x1c  从 layout 起始的偏移
    uint32_t schema_capacity;    // +0x20  Schema 区容量
    uint32_t hash_table_offset;  // +0x24  P1 ⑤：哈希索引区偏移（相对 layout 起始）
    uint32_t field_data_offset;  // +0x28  P1 ⑤：字段数据区偏移
    uint32_t page_table_offset;  // +0x2c  P2 ⑧：分页表偏移
    uint32_t page_count;         // +0x30  P2 ⑧：数据页总数
    uint32_t trie_root_offset;   // +0x34  P3 ⑨：Trie 根节点偏移（0 = 无）
    uint8_t  reserved[8];        // +0x38
    // +0x40
    uint8_t  bufA[64 * 1024];    // 64KB buffer A
    uint8_t  bufB[64 * 1024];    // 64KB buffer B
    // Schema 区紧随 bufB（由 schema_capacity 指定总大小）
};

static constexpr size_t SHM_BUF_SIZE = 64 * 1024;
static constexpr size_t SHM_HEAD_SIZE = offsetof(shm_config_layout, bufA);

/* Schema JSON 区默认容量：2MB（可容纳约 10,000 字段的 JSON 序列化） */
static constexpr size_t DEFAULT_SCHEMA_CAPACITY = 2 * 1024 * 1024;

/* 计算总 SHM 大小 = 头部 + 双缓冲 + Schema 区 */
static inline size_t shm_total_size(size_t schema_cap = DEFAULT_SCHEMA_CAPACITY)
{
    return SHM_HEAD_SIZE + 2 * SHM_BUF_SIZE + schema_cap;
}

class shm_manager {
public:
    shm_manager() = default;
    ~shm_manager();

    shm_manager(const shm_manager&) = delete;
    shm_manager& operator=(const shm_manager&) = delete;

    shm_manager(shm_manager&& other) noexcept;
    shm_manager& operator=(shm_manager&& other) noexcept;

    static std::unique_ptr<shm_manager> create(const std::string& config_key,
                                                size_t schema_capacity = DEFAULT_SCHEMA_CAPACITY);
    static std::unique_ptr<shm_manager> attach(const std::string& config_key);

    void detach();

    shm_config_layout* get_layout() noexcept { return layout_; }
    const shm_config_layout* get_layout() const noexcept { return layout_; }
    const std::string& get_name() const noexcept { return name_; }
    size_t get_total_size() const noexcept { return total_size_; }

    bool is_valid() const noexcept { return layout_ != nullptr; }

private:
    shm_manager(const std::string& config_key);

    std::string name_;
    void* backing_fd_ = nullptr;
    shm_config_layout* layout_ = nullptr;
    size_t total_size_ = 0;
};

/* Schema 区的起始偏移（紧跟 bufB 之后） */
static inline size_t schema_area_offset()
{
    return SHM_HEAD_SIZE + 2 * SHM_BUF_SIZE;
}

/* ── Cross-platform packed struct ───────────────────────────────────── */
#ifdef _MSC_VER
#define BS_PACKED_BEGIN __pragma(pack(push, 1))
#define BS_PACKED_END   __pragma(pack(pop))
#define BS_PACKED
#else
#define BS_PACKED_BEGIN
#define BS_PACKED_END
#define BS_PACKED       __attribute__((packed))
#endif

/* ════════════════════════════════════════════════════════════════
 * P1 ⑤：哈希索引 + 二进制字段记录格式
 * ════════════════════════════════════════════════════════════════ */

BS_PACKED_BEGIN
/* 哈希槽：8 字节 */
struct BS_PACKED hash_slot {
    uint32_t hash_low;       // xxHash 低 32 位（碰撞检测）
    uint32_t data_offset;    // 字段在数据区的偏移（相对 field_data_offset）
};

/* 二进制字段记录头部（紧凑格式，零拷贝可读） */
struct BS_PACKED field_record_header {
    uint16_t key_len;
    uint16_t default_len;
    uint16_t desc_len;
    uint8_t  type;
    uint8_t  required;
};
BS_PACKED_END

/* 哈希索引参数 */
static constexpr size_t HASH_SLOT_COUNT = 4096;
static constexpr size_t HASH_INDEX_SIZE = HASH_SLOT_COUNT * sizeof(hash_slot);
static constexpr uint32_t HASH_SLOT_EMPTY = 0;

/* 计算字段记录总大小（含变长数据） */
static inline size_t field_record_size(const field_record_header* hdr)
{
    return sizeof(field_record_header) + hdr->key_len + hdr->default_len + hdr->desc_len;
}

/* 获取字段记录中各数据的指针 */
static inline const char* field_record_key(const field_record_header* hdr)
{
    return reinterpret_cast<const char*>(hdr + 1);
}
static inline const char* field_record_default(const field_record_header* hdr)
{
    return field_record_key(hdr) + hdr->key_len;
}
static inline const char* field_record_desc(const field_record_header* hdr)
{
    return field_record_default(hdr) + hdr->default_len;
}

/* ── xxHash32 简化实现（无依赖） ───────────────────────────────────── */
static inline uint32_t bs_xxhash32(const void* input, size_t len, uint32_t seed = 0)
{
    /* xxHash32 简化版 — 仅用于哈希索引，不做加密 */
    const uint32_t PRIME32_1 = 2654435761U;
    const uint32_t PRIME32_2 = 2246822519U;
    const uint32_t PRIME32_3 = 3266489917U;
    const uint32_t PRIME32_4 = 668265263U;
    const uint32_t PRIME32_5 = 374761393U;

    const uint8_t* p = (const uint8_t*)input;
    const uint8_t* b = p;
    size_t remaining = len;
    uint32_t h32 = seed + PRIME32_5;

    while (remaining >= 4) {
        uint32_t v = *(const uint32_t*)b;
        v *= PRIME32_2;
        v = (v << 13) | (v >> 19);
        v *= PRIME32_1;
        h32 ^= v;
        h32 = (h32 << 19) | (h32 >> 13);
        h32 = h32 * PRIME32_1 + PRIME32_4;
        b += 4;
        remaining -= 4;
    }

    while (remaining--) {
        h32 ^= *b++ * PRIME32_5;
        h32 = (h32 << 11) | (h32 >> 21);
        h32 *= PRIME32_1;
    }

    h32 ^= (uint32_t)len;
    h32 ^= h32 >> 15;
    h32 *= PRIME32_2;
    h32 ^= h32 >> 13;
    h32 *= PRIME32_3;
    h32 ^= h32 >> 16;
    return h32;
}

/* 哈希索引查找（Editor 侧用） */
static inline int hash_index_lookup(const hash_slot* table, uint32_t num_slots,
                                     const char* key, size_t key_len)
{
    if (!table || !key || num_slots == 0) return -1;
    uint32_t h = bs_xxhash32(key, key_len);
    uint32_t idx = h % num_slots;
    uint32_t start = idx;

    do {
        if (table[idx].hash_low == 0 && table[idx].data_offset == 0)
            return -1;  /* 空槽 → 未命中 */
        if (table[idx].hash_low == h)
            return (int)table[idx].data_offset;
        idx = (idx + 1) % num_slots;
    } while (idx != start);

    return -1;  /* 表满 */
}

/* 哈希索引插入（SDK 序列化时用） */
static inline int hash_index_insert(hash_slot* table, uint32_t num_slots,
                                     const char* key, size_t key_len,
                                     uint32_t data_offset)
{
    uint32_t h = bs_xxhash32(key, key_len);
    uint32_t idx = h % num_slots;
    uint32_t start = idx;

    do {
        if (table[idx].hash_low == 0) {
            table[idx].hash_low = h;
            table[idx].data_offset = data_offset;
            return 0;
        }
        idx = (idx + 1) % num_slots;
    } while (idx != start);

    return -1;  /* 表满 */
}

/* ════════════════════════════════════════════════════════════════
 * P2 ⑧：分页 Schema
 * ════════════════════════════════════════════════════════════════ */
BS_PACKED_BEGIN
/* 页表条目：8 字节 */
struct BS_PACKED page_entry {
    uint32_t page_offset;    /* 数据在 Schema 区内的偏移 */
    uint32_t page_len;       /* 该页数据长度 */
};
BS_PACKED_END

static constexpr size_t FIELDS_PER_PAGE = 64;     /* 每页 64 字段 */
static constexpr size_t PAGE_ENTRY_SIZE = sizeof(page_entry);
static constexpr size_t PAGE_SIZE_BYTES = 4096;    /* 4KB 页对齐 */

/* 计算一个字段记录所需的近似页数 */
static inline size_t field_page_index(size_t field_idx)
{
    return field_idx / FIELDS_PER_PAGE;
}

/* ════════════════════════════════════════════════════════════════
 * P3 ⑨：紧凑 Trie 索引
 * ════════════════════════════════════════════════════════════════ */
BS_PACKED_BEGIN
/* 紧凑 Trie 节点：41 字节 */
struct BS_PACKED trie_node {
    uint8_t  name_len;           /* 节点名长度（1-31） */
    char     name[31];           /* 节点名（如 "db"、"gift"） */
    uint32_t first_child_off;    /* 第一个子节点偏移（从 trie 区起始），0=无 */
    uint32_t next_sibling_off;   /* 兄弟节点偏移，0=无 */
    uint32_t first_field_off;    /* 本节点第一条字段偏移，0=非叶子 */
};
BS_PACKED_END

static constexpr size_t TRIE_NODE_SIZE = sizeof(trie_node);

/* ── Trie 查询：按名找子节点 ───────────────────────────────────────── */
static inline const trie_node* trie_find_child(const trie_node* base,
                                                const trie_node* parent,
                                                const char* name,
                                                size_t name_len)
{
    if (!base || !parent || !name) return nullptr;
    uint32_t off = parent->first_child_off;
    while (off != 0) {
        const trie_node* child = reinterpret_cast<const trie_node*>(
            reinterpret_cast<const uint8_t*>(base) + off);
        if (child->name_len == name_len &&
            memcmp(child->name, name, name_len) == 0) {
            return child;
        }
        off = child->next_sibling_off;
    }
    return nullptr;
}

/* ── Trie 查询：字段级别前缀匹配按名找子节点
 *    （忽略 name_len 限制，仅匹配 name 前缀） ───────────────────── */
static inline const trie_node* trie_find_child_prefix(const trie_node* base,
                                                       const trie_node* parent,
                                                       const char* name,
                                                       size_t name_len)
{
    if (!base || !parent || !name) return nullptr;
    uint32_t off = parent->first_child_off;
    while (off != 0) {
        const trie_node* child = reinterpret_cast<const trie_node*>(
            reinterpret_cast<const uint8_t*>(base) + off);
        size_t min_len = (child->name_len < name_len) ? child->name_len : name_len;
        if (memcmp(child->name, name, min_len) == 0) {
            return child;
        }
        off = child->next_sibling_off;
    }
    return nullptr;
}

} // namespace shm
} // namespace sdk
} // namespace app
} // namespace bs
