/* OPT-08: Gate map hash + upsert test */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bs/kernel/gate_chain/gate_chain_types.h>
#include <bs/kernel/gate_chain/gate_chain_serialize.h>

static int test_map_create_lookup(void)
{
    printf("  test_map_create_lookup ... ");

    bs_gate_map_t* map = NULL;
    int r = bs_gate_map_create(&map, 16);
    if (r != 0) { printf("FAIL: create returned %d\n", r); return 1; }
    if (!map) { printf("FAIL: map is NULL\n"); return 1; }
    if (map->count != 0) { printf("FAIL: count not 0\n"); return 1; }

    /* Insert */
    r = bs_gate_map_insert(map, "default:amount_limit:0:threshold", 0);
    if (r != 0) { printf("FAIL: insert 0 returned %d\n", r); bs_gate_map_free(map); return 1; }
    r = bs_gate_map_insert(map, "production:approval_level:1:approval", 1);
    if (r != 0) { printf("FAIL: insert 1 returned %d\n", r); bs_gate_map_free(map); return 1; }
    r = bs_gate_map_insert(map, "dev:server_port:2:format", 2);
    if (r != 0) { printf("FAIL: insert 2 returned %d\n", r); bs_gate_map_free(map); return 1; }

    if (map->count != 3) { printf("FAIL: count expected 3, got %zu\n", map->count); bs_gate_map_free(map); return 1; }

    /* Lookup */
    size_t idx;
    r = bs_gate_map_lookup(map, "default:amount_limit:0:threshold", &idx);
    if (r != 0 || idx != 0) { printf("FAIL: lookup 0 failed (r=%d, idx=%zu)\n", r, idx); bs_gate_map_free(map); return 1; }

    r = bs_gate_map_lookup(map, "production:approval_level:1:approval", &idx);
    if (r != 0 || idx != 1) { printf("FAIL: lookup 1 failed (r=%d, idx=%zu)\n", r, idx); bs_gate_map_free(map); return 1; }

    r = bs_gate_map_lookup(map, "dev:server_port:2:format", &idx);
    if (r != 0 || idx != 2) { printf("FAIL: lookup 2 failed (r=%d, idx=%zu)\n", r, idx); bs_gate_map_free(map); return 1; }

    /* Lookup non-existent */
    r = bs_gate_map_lookup(map, "nonexistent", &idx);
    if (r == 0) { printf("FAIL: nonexistent lookup should fail\n"); bs_gate_map_free(map); return 1; }

    /* Overwrite */
    r = bs_gate_map_insert(map, "default:amount_limit:0:threshold", 99);
    if (r != 0) { printf("FAIL: overwrite returned %d\n", r); bs_gate_map_free(map); return 1; }
    r = bs_gate_map_lookup(map, "default:amount_limit:0:threshold", &idx);
    if (r != 0 || idx != 99) { printf("FAIL: overwrite verify failed (r=%d, idx=%zu)\n", r, idx); bs_gate_map_free(map); return 1; }

    bs_gate_map_free(map);
    printf("OK\n");
    return 0;
}

static int test_map_rebuild(void)
{
    printf("  test_map_rebuild ... ");

    bs_gate_map_t* map = NULL;
    bs_gate_map_create(&map, 4);

    /* Insert enough to trigger rebuild (load > 0.7) */
    for (size_t i = 0; i < 10; i++) {
        char key[128];
        snprintf(key, sizeof(key), "key_%zu", i);
        int r = bs_gate_map_insert(map, key, i);
        if (r != 0) { printf("FAIL: insert %zu returned %d\n", i, r); bs_gate_map_free(map); return 1; }
    }

    if (map->count != 10) { printf("FAIL: count expected 10, got %zu\n", map->count); bs_gate_map_free(map); return 1; }

    /* Verify all lookups work after rebuild */
    for (size_t i = 0; i < 10; i++) {
        char key[128];
        snprintf(key, sizeof(key), "key_%zu", i);
        size_t idx;
        int r = bs_gate_map_lookup(map, key, &idx);
        if (r != 0 || idx != i) { printf("FAIL: lookup %zu failed after rebuild (r=%d, idx=%zu)\n", i, r, idx); bs_gate_map_free(map); return 1; }
    }

    bs_gate_map_free(map);
    printf("OK\n");
    return 0;
}

static int test_upsert_append(void)
{
    printf("  test_upsert_append ... ");

    bs_gate_chain_t* chain = (bs_gate_chain_t*)calloc(1, sizeof(bs_gate_chain_t));
    chain->version = strdup("1.0");

    bs_gate_node_t node;
    memset(&node, 0, sizeof(node));
    node.type       = strdup("bs_condition");
    node.field_key  = strdup("amount");
    node.op         = strdup("lt");
    node.value      = strdup("50000");
    node.stable_key = strdup("default:amount:0:threshold");
    node.layer      = BS_GATE_LAYER_DEFAULT;

    size_t idx;
    int r = bs_gate_chain_upsert(chain, &node, &idx);
    if (r != 0) { printf("FAIL: upsert append returned %d\n", r); bs_gate_chain_free(chain); return 1; }
    if (idx != 0) { printf("FAIL: upsert append idx expected 0, got %zu\n", idx); bs_gate_chain_free(chain); return 1; }
    if (chain->node_count != 1) { printf("FAIL: node_count expected 1, got %zu\n", chain->node_count); bs_gate_chain_free(chain); return 1; }

    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

static int test_upsert_overwrite(void)
{
    printf("  test_upsert_overwrite ... ");

    bs_gate_chain_t* chain = (bs_gate_chain_t*)calloc(1, sizeof(bs_gate_chain_t));
    chain->version = strdup("1.0");

    bs_gate_node_t node;
    memset(&node, 0, sizeof(node));
    node.type       = strdup("bs_condition");
    node.field_key  = strdup("amount");
    node.op         = strdup("lt");
    node.value      = strdup("50000");
    node.stable_key = strdup("test:amount:0:threshold");
    node.layer      = BS_GATE_LAYER_DEFAULT;

    size_t idx;
    bs_gate_chain_upsert(chain, &node, &idx);

    /* Upsert again with same stable_key but different value */
    bs_gate_node_t node2;
    memset(&node2, 0, sizeof(node2));
    node2.type       = strdup("bs_condition");
    node2.field_key  = strdup("amount");
    node2.op         = strdup("lt");
    node2.value      = strdup("100000");
    node2.stable_key = strdup("test:amount:0:threshold");
    node2.layer      = BS_GATE_LAYER_DEFAULT;

    size_t idx2;
    int r = bs_gate_chain_upsert(chain, &node2, &idx2);
    if (r != 0) { printf("FAIL: upsert overwrite returned %d\n", r); bs_gate_chain_free(chain); return 1; }
    if (idx2 != 0) { printf("FAIL: upsert overwrite idx expected 0, got %zu\n", idx2); bs_gate_chain_free(chain); return 1; }
    if (chain->node_count != 1) { printf("FAIL: node_count should still be 1, got %zu\n", chain->node_count); bs_gate_chain_free(chain); return 1; }

    /* Verify overwritten value */
    if (strcmp(chain->nodes[0].value, "100000") != 0) {
        printf("FAIL: overwrite value expected '100000', got '%s'\n", chain->nodes[0].value);
        bs_gate_chain_free(chain); return 1;
    }

    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

static int test_serialize_with_layer(void)
{
    printf("  test_serialize_with_layer ... ");

    bs_gate_chain_t* chain = (bs_gate_chain_t*)calloc(1, sizeof(bs_gate_chain_t));
    chain->version = strdup("1.0");
    chain->node_count = 1;
    chain->nodes = (bs_gate_node_t*)calloc(1, sizeof(bs_gate_node_t));
    chain->nodes[0].type         = strdup("bs_condition");
    chain->nodes[0].id           = strdup("g1");
    chain->nodes[0].field_key    = strdup("amount");
    chain->nodes[0].op           = strdup("lt");
    chain->nodes[0].value        = strdup("50000");
    chain->nodes[0].layer        = BS_GATE_LAYER_DEFAULT;
    chain->nodes[0].stable_key   = strdup("default:amount:0:threshold");
    chain->nodes[0].sub_category = strdup("threshold");
    chain->nodes[0].domain       = strdup("default");
    chain->nodes[0].entity       = strdup("amount");

    char* json = NULL;
    size_t jlen = 0;
    int r = bs_gate_chain_to_json(chain, &json, &jlen);
    if (r != 0) { printf("FAIL: to_json returned %d\n", r); bs_gate_chain_free(chain); return 1; }
    if (!json || jlen == 0) { printf("FAIL: empty json\n"); bs_gate_chain_free(chain); return 1; }

    /* Deserialize and verify layer field */
    bs_gate_chain_t* chain2 = NULL;
    r = bs_gate_chain_from_json(json, &chain2);
    if (r != 0) { printf("FAIL: from_json returned %d\n", r); free(json); bs_gate_chain_free(chain); return 1; }
    if (chain2->node_count != 1) { printf("FAIL: node_count mismatch\n"); free(json); bs_gate_chain_free(chain); bs_gate_chain_free(chain2); return 1; }
    if (chain2->nodes[0].layer != BS_GATE_LAYER_DEFAULT) {
        printf("FAIL: layer expected %d, got %d\n", BS_GATE_LAYER_DEFAULT, chain2->nodes[0].layer);
        free(json); bs_gate_chain_free(chain); bs_gate_chain_free(chain2); return 1;
    }
    if (!chain2->nodes[0].stable_key || strcmp(chain2->nodes[0].stable_key, "default:amount:0:threshold") != 0) {
        printf("FAIL: stable_key mismatch\n");
        free(json); bs_gate_chain_free(chain); bs_gate_chain_free(chain2); return 1;
    }

    free(json);
    bs_gate_chain_free(chain);
    bs_gate_chain_free(chain2);
    printf("OK\n");
    return 0;
}

int main(void)
{
    printf("gate_chain_map_upsert_test:\n");
    int fail = 0;
    fail += test_map_create_lookup();
    fail += test_map_rebuild();
    fail += test_upsert_append();
    fail += test_upsert_overwrite();
    fail += test_serialize_with_layer();
    printf("  %s\n", fail ? "FAILED" : "ALL PASS");
    return fail ? 1 : 0;
}
