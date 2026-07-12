/**
 * Manifest Loader Unit Tests.
 *
 * ADR: 业务系统目录设计与配置注册规范化 (方案C)
 * 覆盖:
 *   - bs_manifest_load_from_json 正例
 *   - bs_manifest_load_from_json 缺少必需字段
 *   - bs_manifest_load_from_json fields[] 解析
 *   - bs_manifest_load_from_json ai_data 解析
 *   - bs_manifest_load_from_file 文件不存在
 *   - bs_manifest_load_from_json NULL 参数
 */

#include "bs/adapter/business/manifest_loader.h"
#include "bs/adapter/business/manifest.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int test_count = 0;
static int pass_count = 0;

#define TEST(name)                                                     \
    do {                                                               \
        test_count++;                                                  \
        fprintf(stderr, "  [TEST] %-60s ", name);                      \
    } while (0)

#define PASS()                                                         \
    do {                                                               \
        pass_count++;                                                  \
        fprintf(stderr, "PASS\n");                                     \
    } while (0)

#define FAIL(msg)                                                      \
    do {                                                               \
        fprintf(stderr, "FAIL: %s\n", msg);                            \
    } while (0)

#define ASSERT_EQ(a, b)                                                \
    do { if ((a) != (b)) { FAIL(#a " == " #b); return; } } while (0)

#define ASSERT_TRUE(c)                                                 \
    do { if (!(c)) { FAIL(#c); return; } } while (0)

#define ASSERT_FALSE(c)                                                \
    do { if (c) { FAIL("!" #c); return; } } while (0)

#define ASSERT_STR_EQ(a, b)                                            \
    do { if (strcmp((a), (b)) != 0) { FAIL(#a " == " #b); return; } } while (0)

#define ASSERT_NULL(p)                                                 \
    do { if ((p) != NULL) { FAIL(#p " is not NULL"); return; } } while (0)

/* ─── 测试用 JSON ────────────────────────────────────────────────────── */

static const char* FULL_JSON =
    "{"
    "  \"biz_id\": \"test-biz\","
    "  \"display_name\": \"测试系统\","
    "  \"sdk_version\": \">=1.0.0 <2.0.0\","
    "  \"normalizer_lib_path\": \"libs/norm.dll\","
    "  \"fields\": ["
    "    {\"key\": \"room_id\", \"type\": \"string\", \"default\": \"\", \"description\": \"房间号\", \"required\": true},"
    "    {\"key\": \"font_size\", \"type\": \"int32\", \"default\": \"14\"}"
    "  ],"
    "  \"ai_data\": {"
    "    \"config_labels\": {"
    "      \"label_a\": \"value_a\","
    "      \"label_b\": \"value_b\""
    "    },"
    "    \"inverted_index\": ["
    "      {\"keyword\": \"弹幕\", \"config_keys\": [\"font_size\", \"opacity\"]}"
    "    ],"
    "    \"domain_shards\": ["
    "      {\"domain_name\": \"danmaku\", \"keywords\": [\"danmaku\",\"弹幕\"], \"domain_description\": \"弹幕业务域\"}"
    "    ],"
    "    \"skill_routes\": ["
    "      {\"prefix\": \"danmaku\", \"description\": \"弹幕技能\", \"priority\": 1, \"tool_chain\": [\"LLM\"]}"
    "    ]"
    "  }"
    "}";

/* ─── Tests ───────────────────────────────────────────────────────────── */

static void test_load_basic() {
    TEST("manifest_load_from_json 基础解析");
    bs_manifest_t m;
    int rc = bs_manifest_load_from_json(FULL_JSON, &m);
    ASSERT_EQ(0, rc);
    ASSERT_STR_EQ("test-biz", m.biz_id);
    ASSERT_STR_EQ("测试系统", m.display_name);
    ASSERT_STR_EQ(">=1.0.0 <2.0.0", m.sdk_version);
    ASSERT_STR_EQ("libs/norm.dll", m.normalizer_lib_path);
    bs_manifest_destroy(&m);
    PASS();
}

static void test_load_fields() {
    TEST("manifest_load_from_json fields 解析");
    bs_manifest_t m;
    ASSERT_EQ(0, bs_manifest_load_from_json(FULL_JSON, &m));
    ASSERT_EQ(2, (int)m.fields_count);
    ASSERT_STR_EQ("room_id", m.fields[0].key);
    ASSERT_STR_EQ("string", m.fields[0].type);
    ASSERT_STR_EQ("", m.fields[0].default_str);
    ASSERT_EQ(1, m.fields[0].required);
    ASSERT_STR_EQ("font_size", m.fields[1].key);
    ASSERT_STR_EQ("int32", m.fields[1].type);
    ASSERT_STR_EQ("14", m.fields[1].default_str);
    ASSERT_EQ(0, m.fields[1].required);
    bs_manifest_destroy(&m);
    PASS();
}

static void test_load_ai_labels() {
    TEST("manifest_load_from_json config_labels 解析");
    bs_manifest_t m;
    ASSERT_EQ(0, bs_manifest_load_from_json(FULL_JSON, &m));
    ASSERT_TRUE(m.ai_data.config_labels.count > 0);
    ASSERT_STR_EQ("label_a", m.ai_data.config_labels.keys[0]);
    ASSERT_STR_EQ("value_a", m.ai_data.config_labels.values[0]);
    bs_manifest_destroy(&m);
    PASS();
}

static void test_load_inverted_index() {
    TEST("manifest_load_from_json inverted_index 解析");
    bs_manifest_t m;
    ASSERT_EQ(0, bs_manifest_load_from_json(FULL_JSON, &m));
    ASSERT_EQ(1, (int)m.ai_data.inverted_index_count);
    ASSERT_STR_EQ("弹幕", m.ai_data.inverted_index[0].keyword);
    ASSERT_EQ(2, (int)m.ai_data.inverted_index[0].keys_count);
    ASSERT_STR_EQ("font_size", m.ai_data.inverted_index[0].config_keys[0]);
    bs_manifest_destroy(&m);
    PASS();
}

static void test_load_domain_shards() {
    TEST("manifest_load_from_json domain_shards 解析");
    bs_manifest_t m;
    ASSERT_EQ(0, bs_manifest_load_from_json(FULL_JSON, &m));
    ASSERT_EQ(1, (int)m.ai_data.domain_shards_count);
    ASSERT_STR_EQ("danmaku", m.ai_data.domain_shards[0].domain_name);
    ASSERT_STR_EQ("弹幕业务域", m.ai_data.domain_shards[0].domain_description);
    ASSERT_EQ(2, (int)m.ai_data.domain_shards[0].keywords_count);
    bs_manifest_destroy(&m);
    PASS();
}

static void test_load_skill_routes() {
    TEST("manifest_load_from_json skill_routes 解析");
    bs_manifest_t m;
    ASSERT_EQ(0, bs_manifest_load_from_json(FULL_JSON, &m));
    ASSERT_EQ(1, (int)m.ai_data.skill_routes_count);
    ASSERT_STR_EQ("danmaku", m.ai_data.skill_routes[0].prefix);
    ASSERT_EQ(1, m.ai_data.skill_routes[0].priority);
    ASSERT_EQ(1, (int)m.ai_data.skill_routes[0].tool_chain_count);
    ASSERT_STR_EQ("LLM", m.ai_data.skill_routes[0].tool_chain[0]);
    bs_manifest_destroy(&m);
    PASS();
}

static void test_load_missing_biz_id() {
    TEST("manifest_load_from_json 缺少 biz_id 返回 -1");
    bs_manifest_t m;
    const char* bad_json = "{\"display_name\": \"no-id\"}";
    int rc = bs_manifest_load_from_json(bad_json, &m);
    ASSERT_EQ(-1, rc);
    PASS();
}

static void test_load_missing_display() {
    TEST("manifest_load_from_json 缺少 display_name 返回 -1");
    bs_manifest_t m;
    const char* bad_json = "{\"biz_id\": \"no-display\"}";
    int rc = bs_manifest_load_from_json(bad_json, &m);
    ASSERT_EQ(-1, rc);
    PASS();
}

static void test_load_null() {
    TEST("manifest_load_from_json(NULL) 返回 -1");
    bs_manifest_t m;
    ASSERT_EQ(-1, bs_manifest_load_from_json(NULL, &m));
    PASS();
}

static void test_load_empty_json() {
    TEST("manifest_load_from_json({}) 返回 -1");
    bs_manifest_t m;
    ASSERT_EQ(-1, bs_manifest_load_from_json("{}", &m));
    PASS();
}

static void test_load_minimal() {
    TEST("manifest_load_from_json 最小合法 JSON");
    bs_manifest_t m;
    const char* minimal = "{\"biz_id\": \"min\", \"display_name\": \"最小系统\"}";
    ASSERT_EQ(0, bs_manifest_load_from_json(minimal, &m));
    ASSERT_STR_EQ("min", m.biz_id);
    ASSERT_EQ(0, (int)m.fields_count);
    ASSERT_NULL(m.fields);
    bs_manifest_destroy(&m);
    PASS();
}

static void test_load_file_not_found() {
    TEST("manifest_load_from_file 不存在返回 -1");
    bs_manifest_t m;
    int rc = bs_manifest_load_from_file("/nonexistent/path/manifest.json", &m);
    ASSERT_EQ(-1, rc);
    PASS();
}

/* ─── Main ────────────────────────────────────────────────────────────── */

int main() {
    fprintf(stderr, "\n=== Business: Manifest Loader Tests ===\n\n");

    test_load_basic();
    test_load_fields();
    test_load_ai_labels();
    test_load_inverted_index();
    test_load_domain_shards();
    test_load_skill_routes();
    test_load_missing_biz_id();
    test_load_missing_display();
    test_load_null();
    test_load_empty_json();
    test_load_minimal();
    test_load_file_not_found();

    fprintf(stderr, "\n  Results: %d / %d passed\n\n",
            pass_count, test_count);

    return (pass_count == test_count) ? 0 : 1;
}
