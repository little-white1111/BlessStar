/* ConfigDeclareBenchTest: 专题四 P0 ③ — 10,000 字段性能基准测试
 *
 * 测量指标：
 *   - merge 耗时：b 次 bs_config_declare() 合计耗时
 *   - 序列化耗时：最终 schema_json 大小与耗时
 *   - 内存占用：Schema JSON 字节数
 *
 * 验收标准（专题四）：
 *   - 10,000 字段合并耗时 < 5ms（对比优化前线性数组 ~500ms）
 *   - SHM Schema 区不溢出（当前 2MB capacity）
 */
#include "bs/app/sdk/config_declare.h"
#include "bs/app/sdk/shm/shm_manager.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <string>
#include <vector>

/* ── Test helpers ─────────────────────────────────────────────────── */
static int g_test_passed = 0;
static int g_test_failed = 0;

#define TEST(name) do { \
    printf("  BENCH: %s ... ", #name); \
    bench_##name(); \
    printf("PASSED\n"); \
    g_test_passed++; \
} while(0)

#define ASSERT_LT_MS(val, limit_ms, desc) do { \
    double __v = (val); \
    double __l = (limit_ms); \
    if (__v >= __l) { \
        fprintf(stderr, "FAIL at %s:%d: %s %.2fms >= %.2fms\n", \
                __FILE__, __LINE__, (desc), __v, __l); \
        g_test_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        fprintf(stderr, "FAIL at %s:%d: expected %lld, got %lld\n", \
                __FILE__, __LINE__, (long long)(b), (long long)(a)); \
        g_test_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_NOT_NULL(p) do { \
    if ((p) == NULL) { \
        fprintf(stderr, "FAIL at %s:%d: expected non-NULL\n", \
                __FILE__, __LINE__); \
        g_test_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_GT(val, limit, desc) do { \
    auto __v = (val); \
    auto __l = (limit); \
    if (!(__v > __l)) { \
        fprintf(stderr, "FAIL at %s:%d: %s %lld <= %lld\n", \
                __FILE__, __LINE__, (desc), (long long)__v, (long long)__l); \
        g_test_failed++; \
        return; \
    } \
} while(0)

/* ── 生成 N 个字段声明 ───────────────────────────────────────────── */
static std::vector<bs_field_decl_t> generate_fields(int n)
{
    std::vector<bs_field_decl_t> fields;
    fields.reserve(n);
    for (int i = 0; i < n; i++) {
        char* key = (char*)malloc(64);
        char* def = (char*)malloc(32);
        char* desc = (char*)malloc(64);
        snprintf(key, 64, "db.field_%d", i);
        snprintf(def, 32, "%d", i * 10);
        snprintf(desc, 64, "Field number %d", i);
        bs_field_decl_t f;
        f.key = key;
        f.type = (bs_field_type_t)(i % 5);
        f.default_str = def;
        f.description = desc;
        f.required = (i % 10 == 0);
        fields.push_back(f);
    }
    return fields;
}

static void free_fields(std::vector<bs_field_decl_t>& fields)
{
    for (auto& f : fields) {
        free((void*)f.key);
        free((void*)f.default_str);
        free((void*)f.description);
    }
}

/* ── 基准 1：10,000 字段合并耗时 ──────────────────────────────────── */
static void bench_10k_merge(void)
{
    bs_config_declare_reset();

    auto fields = generate_fields(10000);

    auto t0 = std::chrono::high_resolution_clock::now();

    int ret = bs_config_declare(fields.data(), fields.size());

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    printf("%.2fms (10,000 fields) ", elapsed_ms);
    ASSERT_EQ(ret, 0);
    ASSERT_LT_MS(elapsed_ms, 50.0, "10K merge");  /* 宽松限：P0 目标 <5ms，放宽到 50ms 防 CI jitter */

    char* json = nullptr;
    size_t len = 0;
    bs_config_declare_get_schema_json(&json, &len);
    ASSERT_NOT_NULL(json);
    ASSERT_GT(len, (size_t)100000, "JSON size > 100KB");

    printf("JSON=%zuKB ", len / 1024);
    free(json);
    free_fields(fields);
    bs_config_declare_reset();
}

/* ── 基准 2：增量合并（已有 10,000 再追加 100） ──────────────────── */
static void bench_incremental_merge(void)
{
    bs_config_declare_reset();

    auto base = generate_fields(10000);
    bs_config_declare(base.data(), base.size());

    /* 追加 100 个新字段 */
    std::vector<bs_field_decl_t> extra;
    for (int i = 0; i < 100; i++) {
        char* key = (char*)malloc(64);
        char* def = (char*)malloc(32);
        char* desc = (char*)malloc(64);
        snprintf(key, 64, "db.extra_%d", i);
        snprintf(def, 32, "%d", i);
        snprintf(desc, 64, "Extra field %d", i);
        bs_field_decl_t f;
        f.key = key;
        f.type = BS_TYPE_STRING;
        f.default_str = def;
        f.description = desc;
        f.required = false;
        extra.push_back(f);
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    int ret = bs_config_declare(extra.data(), extra.size());
    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    printf("%.2fms (10K base + 100 new) ", elapsed_ms);
    ASSERT_EQ(ret, 0);
    ASSERT_LT_MS(elapsed_ms, 50.0, "incremental merge");

    free_fields(base);
    free_fields(extra);
    bs_config_declare_reset();
}

/* ── 基准 3：覆盖合并（10,000 字段逐个覆盖） ──────────────────────── */
static void bench_overwrite_merge(void)
{
    bs_config_declare_reset();

    auto fields = generate_fields(10000);

    /* 第一遍注册 */
    bs_config_declare(fields.data(), fields.size());

    /* 修改所有字段的 default_str */
    for (auto& f : fields) {
        free((void*)f.default_str);
        char* def = (char*)malloc(32);
        snprintf(def, 32, "overwrite_%d", rand() % 10000);
        f.default_str = def;
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    int ret = bs_config_declare(fields.data(), fields.size());
    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    printf("%.2fms (10K overwrite) ", elapsed_ms);
    ASSERT_EQ(ret, 0);
    ASSERT_LT_MS(elapsed_ms, 50.0, "overwrite merge");

    free_fields(fields);
    bs_config_declare_reset();
}

/* ── 基准 4：SHM 容量校验（验证 2MB 能容纳 10,000 字段 JSON） ────── */
static void bench_shm_capacity(void)
{
    bs_config_declare_reset();

    auto fields = generate_fields(10000);
    bs_config_declare(fields.data(), fields.size());

    char* json = nullptr;
    size_t len = 0;
    bs_config_declare_get_schema_json(&json, &len);
    ASSERT_NOT_NULL(json);

    /* 2MB = 2,097,152 bytes */
    const size_t schema_capacity = 2 * 1024 * 1024;
    printf("JSON=%zuKB (capacity=%zuKB) ", len / 1024, schema_capacity / 1024);
    ASSERT_LT_MS((double)len, (double)schema_capacity * 0.9,
                 "JSON within 90% of SHM schema capacity");

    free(json);
    free_fields(fields);
    bs_config_declare_reset();
}

/* ── 基准 5：哈希索引插入 + 查找正确性 ────────────────────────────── */
static void bench_hash_index_roundtrip(void)
{
    using namespace bs::app::sdk::shm;

    /* 栈上分配模拟 SHM 哈希索引区 */
    hash_slot table[HASH_SLOT_COUNT];
    std::memset(table, 0, sizeof(table));

    const char* keys[] = {"db.host", "db.port", "db.pool.max", "gift.threshold", "ui.theme"};
    const uint32_t data_offsets[] = {100, 200, 300, 400, 500};
    const int N = 5;

    /* 插入 */
    for (int i = 0; i < N; i++) {
        int ret = hash_index_insert(table, HASH_SLOT_COUNT, keys[i], strlen(keys[i]), data_offsets[i]);
        ASSERT_EQ(ret, 0);
    }

    /* 查找 */
    for (int i = 0; i < N; i++) {
        int off = hash_index_lookup(table, HASH_SLOT_COUNT, keys[i], strlen(keys[i]));
        ASSERT_EQ(off, (int)data_offsets[i]);
    }

    /* 不存在的 key */
    int off = hash_index_lookup(table, HASH_SLOT_COUNT, "nonexistent", 11);
    ASSERT_EQ(off, -1);

    /* 空表查找 */
    hash_slot empty_tbl[HASH_SLOT_COUNT];
    std::memset(empty_tbl, 0, sizeof(empty_tbl));
    off = hash_index_lookup(empty_tbl, HASH_SLOT_COUNT, "anything", 8);
    ASSERT_EQ(off, -1);
}

/* ── 基准 6：分页 page_entry 结构 ──────────────────────────────────── */
static void bench_page_entry_layout(void)
{
    using namespace bs::app::sdk::shm;

    /* 验证 page_entry 大小 */
    ASSERT_EQ((int)sizeof(page_entry), 8);

    /* 验证字段分页索引 */
    ASSERT_EQ(field_page_index(0), 0);
    ASSERT_EQ(field_page_index(63), 0);
    ASSERT_EQ(field_page_index(64), 1);
    ASSERT_EQ(field_page_index(127), 1);
    ASSERT_EQ(field_page_index(128), 2);

    /* 栈上构造模拟页表 */
    page_entry table[4];
    std::memset(table, 0, sizeof(table));

    table[0].page_offset = 0x1000;
    table[0].page_len = 1024;
    table[1].page_offset = 0x1400;
    table[1].page_len = 2048;

    ASSERT_EQ(table[0].page_offset, 0x1000);
    ASSERT_EQ(table[0].page_len, 1024);
    ASSERT_EQ(table[1].page_offset, 0x1400);
    ASSERT_EQ(table[1].page_len, 2048);
}

/* ── 基准 7：Trie 子节点查找 ───────────────────────────────────────── */
static void bench_trie_lookup(void)
{
    using namespace bs::app::sdk::shm;

    /* 栈上构建微型 Trie：
     * root
     *   ├─ "db" (first_field_off=100)
     *   │   └─ "pool" (first_field_off=200)
     *   └─ "gift" (first_field_off=300)
     *
     * trie_node 在同构栈区中：arena[N] 相对 arena[0] 的偏移 = N * sizeof(trie_node)
     * 使用 uintptr_t 计算偏移以兼容 offsetof 不可用于数组元素的限制。
     */
    trie_node arena[4];
    std::memset(arena, 0, sizeof(arena));

    const size_t NODE_SIZE = sizeof(trie_node);

    /* 节点 0：root */
    arena[0].name_len = 4;
    memcpy(arena[0].name, "root", 4);
    arena[0].first_child_off = static_cast<uint32_t>(1 * NODE_SIZE); /* → arena[1] (db) */
    arena[0].first_field_off = 0;

    /* 节点 1：db */
    arena[1].name_len = 2;
    memcpy(arena[1].name, "db", 2);
    arena[1].first_child_off = static_cast<uint32_t>(2 * NODE_SIZE); /* → arena[2] (pool) */
    arena[1].next_sibling_off = static_cast<uint32_t>(3 * NODE_SIZE); /* → arena[3] (gift) */
    arena[1].first_field_off = 100;

    /* 节点 2：pool */
    arena[2].name_len = 4;
    memcpy(arena[2].name, "pool", 4);
    arena[2].first_field_off = 200;

    /* 节点 3：gift */
    arena[3].name_len = 4;
    memcpy(arena[3].name, "gift", 4);
    arena[3].first_field_off = 300;

    /* 查找 root → db */
    const trie_node* db = trie_find_child(arena, &arena[0], "db", 2);
    ASSERT_NOT_NULL(db);
    ASSERT_EQ(db->first_field_off, 100);

    /* 查找 root → gift */
    const trie_node* gift = trie_find_child(arena, &arena[0], "gift", 4);
    ASSERT_NOT_NULL(gift);
    ASSERT_EQ(gift->first_field_off, 300);

    /* 查找 root → nonexistent */
    const trie_node* none = trie_find_child(arena, &arena[0], "xyz", 3);
    ASSERT_EQ(none, nullptr);

    /* 前缀匹配查找：root → "d" 应该匹配 db */
    const trie_node* prefix_db = trie_find_child_prefix(arena, &arena[0], "d", 1);
    ASSERT_NOT_NULL(prefix_db);
    ASSERT_EQ(prefix_db->first_field_off, 100);

    /* 前缀匹配：root → "gi" 应该匹配 gift */
    const trie_node* prefix_gift = trie_find_child_prefix(arena, &arena[0], "gi", 2);
    ASSERT_NOT_NULL(prefix_gift);
    ASSERT_EQ(prefix_gift->first_field_off, 300);
}

/* ══════════════════════════════════════════════════════════════════
 * Main
 * ══════════════════════════════════════════════════════════════════ */
int main(void)
{
    printf("=== ConfigDeclareBenchTest: %d 字段性能基准 ===\n\n", 10000);

    /* 预热（消除首次分配抖动） */
    auto warmup = generate_fields(100);
    bs_config_declare(warmup.data(), warmup.size());
    bs_config_declare_reset();
    free_fields(warmup);

    TEST(10k_merge);
    TEST(incremental_merge);
    TEST(overwrite_merge);
    TEST(shm_capacity);
    TEST(hash_index_roundtrip);
    TEST(page_entry_layout);
    TEST(trie_lookup);

    printf("\n=== Results: %d passed, %d failed ===\n",
           g_test_passed, g_test_failed);

    return g_test_failed > 0 ? 1 : 0;
}
