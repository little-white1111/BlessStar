/* OPT-03: Agent index export tests */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bs/kernel/schema/schema_registry.h>
#include <bs/kernel/schema/schema_types.h>
#include <bs/kernel/gate_chain/gate_chain_types.h>
#include <bs/agent_indexer.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define file_exists(p) (_access(p, 0) == 0)
#else
#include <unistd.h>
#define file_exists(p) (access(p, F_OK) == 0)
#endif

static int test_export_full(void)
{
    printf("  test_export_full ... ");

    /* Create schema registry with entries */
    bs_schema_registry_t* reg = bs_schema_registry_create();

    bs_schema_field_def_t fields[2];
    memset(&fields[0], 0, sizeof(bs_schema_field_def_t));
    fields[0].name = "amount_limit";
    fields[0].type = BS_SCHEMA_TYPE_I32;
    fields[0].required = true;
    fields[0].ai_hint = "单笔金额上限，单位元，必填，范围[0,99999999]";
    fields[0].range.has_min = true; fields[0].range.min = 0;
    fields[0].range.has_max = true; fields[0].range.max = 99999999;
    fields[0].ui_label = "金额上限";

    memset(&fields[1], 0, sizeof(bs_schema_field_def_t));
    fields[1].name = "approval_level";
    fields[1].type = BS_SCHEMA_TYPE_ENUM;
    fields[1].ai_hint = "审批等级，enum: low/medium/high";
    fields[1].required = true;
    fields[1].ui_label = "审批等级";
    static const char* enum_vals[] = {"low", "medium", "high", NULL};
    fields[1].enum_values = enum_vals;

    bs_schema_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.schema_id  = "biz.finance";
    entry.version    = "1.0";
    entry.root_fields = fields;
    entry.root_count  = 2;
    entry.ui_meta.title = "Finance Config";
    entry.ui_meta.description = "财务配置";

    assert(bs_schema_register(reg, &entry) == BS_SCHEMA_OK);

    /* Create gate chain (DAG-based API) */
    bs_gate_chain_t* chain = bs_gate_chain_create();
    chain->version = strdup("1.0");

    /* Create nodes */
    bs_gate_node_t* node0 = bs_gate_node_create("bs_condition", "g1");
    node0->field_key = strdup("amount_limit");
    node0->op        = strdup("lt");
    node0->value     = strdup("50000");

    bs_gate_node_t* node1 = bs_gate_node_create("bs_meta_rule", "g2");
    node1->field_key = strdup("approval_level");
    node1->op        = strdup("required");
    node1->value     = strdup("high");

    /* Link: g1 DO → g2 */
    bs_gate_node_link_do(node0, node1);

    /* Set root (DFS traversal entry point) */
    chain->root = node0;

    /* Export */
    const char* out_dir = "build/test_agent_index_tmp";
    bs_agent_index_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.output_dir       = out_dir;
    cfg.business_name    = "biz-finance";
    cfg.include_ai_hints = true;
    cfg.include_gate_chain = true;

    int r = bs_agent_index_export(reg, chain, &cfg);
    if (r != 0) { printf("FAIL: export returned %d\n", r); bs_gate_chain_free(chain); bs_schema_registry_destroy(reg); return 1; }

    /* Verify files exist */
    char path[512];
    snprintf(path, sizeof(path), "%s/domain_knowledge.json", out_dir);
    if (!file_exists(path)) { printf("FAIL: domain_knowledge.json not found\n"); bs_gate_chain_free(chain); bs_schema_registry_destroy(reg); return 1; }

    snprintf(path, sizeof(path), "%s/constraint_knowledge.json", out_dir);
    if (!file_exists(path)) { printf("FAIL: constraint_knowledge.json not found\n"); bs_gate_chain_free(chain); bs_schema_registry_destroy(reg); return 1; }

    snprintf(path, sizeof(path), "%s/field_semantics.json", out_dir);
    if (!file_exists(path)) { printf("FAIL: field_semantics.json not found\n"); bs_gate_chain_free(chain); bs_schema_registry_destroy(reg); return 1; }

    bs_gate_chain_free(chain);
    bs_schema_registry_destroy(reg);
    printf("OK\n");
    return 0;
}

static int test_export_schema_only(void)
{
    printf("  test_export_schema_only ... ");

    bs_schema_registry_t* reg = bs_schema_registry_create();
    bs_schema_field_def_t f;
    memset(&f, 0, sizeof(f)); f.name = "x"; f.type = BS_SCHEMA_TYPE_STR; f.ai_hint = "field x hint";
    bs_schema_entry_t e;
    memset(&e, 0, sizeof(e)); e.schema_id = "test"; e.version = "1.0"; e.root_fields = &f; e.root_count = 1;
    assert(bs_schema_register(reg, &e) == BS_SCHEMA_OK);

    const char* out_dir = "build/test_agent_index_tmp2";
    bs_agent_index_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.output_dir        = out_dir;
    cfg.business_name     = "test";
    cfg.include_ai_hints  = true;
    cfg.include_gate_chain = false;

    int r = bs_agent_index_export_schema_only(reg, &cfg);
    if (r != 0) { printf("FAIL: export_schema_only returned %d\n", r); bs_schema_registry_destroy(reg); return 1; }

    char path[512];
    snprintf(path, sizeof(path), "%s/domain_knowledge.json", out_dir);
    if (!file_exists(path)) { printf("FAIL: domain_knowledge.json not found\n"); bs_schema_registry_destroy(reg); return 1; }

    /* constraint_knowledge.json should NOT exist when include_gate_chain is false */
    snprintf(path, sizeof(path), "%s/constraint_knowledge.json", out_dir);
    if (file_exists(path)) { printf("FAIL: constraint_knowledge.json should not exist\n"); bs_schema_registry_destroy(reg); return 1; }

    snprintf(path, sizeof(path), "%s/field_semantics.json", out_dir);
    if (!file_exists(path)) { printf("FAIL: field_semantics.json not found\n"); bs_schema_registry_destroy(reg); return 1; }

    bs_schema_registry_destroy(reg);
    printf("OK\n");
    return 0;
}

int main(void)
{
    printf("agent_index_export_test:\n");
    int fail = 0;
    fail += test_export_full();
    fail += test_export_schema_only();
    printf("  %s\n", fail ? "FAILED" : "ALL PASS");
    return fail ? 1 : 0;
}
