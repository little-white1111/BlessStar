/* DAG version: Gate map hash + upsert test */
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

    bs_gate_node_t n1, n2, n3;
    memset(&n1, 0, sizeof(n1));
    memset(&n2, 0, sizeof(n2));
    memset(&n3, 0, sizeof(n3));

    r = bs_gate_map_insert(map, "default:amount_limit:0:threshold", &n1);
    if (r != 0) { printf("FAIL: insert 0 returned %d\n", r); bs_gate_map_free(map); return 1; }
    r = bs_gate_map_insert(map, "production:approval_level:1:approval", &n2);
    if (r != 0) { printf("FAIL: insert 1 returned %d\n", r); bs_gate_map_free(map); return 1; }
    r = bs_gate_map_insert(map, "dev:server_port:2:format", &n3);
    if (r != 0) { printf("FAIL: insert 2 returned %d\n", r); bs_gate_map_free(map); return 1; }

    if (map->count != 3) { printf("FAIL: count expected 3, got %zu\n", map->count); bs_gate_map_free(map); return 1; }

    bs_gate_node_t* ptr = NULL;
    r = bs_gate_map_lookup(map, "default:amount_limit:0:threshold", &ptr);
    if (r != 0 || ptr != &n1) { printf("FAIL: lookup 0 failed\n"); bs_gate_map_free(map); return 1; }

    r = bs_gate_map_lookup(map, "production:approval_level:1:approval", &ptr);
    if (r != 0 || ptr != &n2) { printf("FAIL: lookup 1 failed\n"); bs_gate_map_free(map); return 1; }

    r = bs_gate_map_lookup(map, "nonexistent", &ptr);
    if (r == 0) { printf("FAIL: nonexistent lookup should fail\n"); bs_gate_map_free(map); return 1; }

    /* Overwrite */
    bs_gate_node_t n_overwrite;
    memset(&n_overwrite, 0, sizeof(n_overwrite));
    r = bs_gate_map_insert(map, "default:amount_limit:0:threshold", &n_overwrite);
    if (r != 0) { printf("FAIL: overwrite returned %d\n", r); bs_gate_map_free(map); return 1; }
    r = bs_gate_map_lookup(map, "default:amount_limit:0:threshold", &ptr);
    if (r != 0 || ptr != &n_overwrite) { printf("FAIL: overwrite verify failed\n"); bs_gate_map_free(map); return 1; }

    bs_gate_map_free(map);
    printf("OK\n");
    return 0;
}

static int test_map_rebuild(void)
{
    printf("  test_map_rebuild ... ");

    bs_gate_map_t* map = NULL;
    bs_gate_map_create(&map, 4);

    bs_gate_node_t nodes[10];
    memset(nodes, 0, sizeof(nodes));

    for (size_t i = 0; i < 10; i++) {
        char key[128];
        snprintf(key, sizeof(key), "key_%zu", i);
        int r = bs_gate_map_insert(map, key, &nodes[i]);
        if (r != 0) { printf("FAIL: insert %zu returned %d\n", i, r); bs_gate_map_free(map); return 1; }
    }

    if (map->count != 10) { printf("FAIL: count expected 10, got %zu\n", map->count); bs_gate_map_free(map); return 1; }

    for (size_t i = 0; i < 10; i++) {
        char key[128];
        snprintf(key, sizeof(key), "key_%zu", i);
        bs_gate_node_t* ptr = NULL;
        int r = bs_gate_map_lookup(map, key, &ptr);
        if (r != 0 || ptr != &nodes[i]) { printf("FAIL: lookup %zu failed after rebuild\n", i); bs_gate_map_free(map); return 1; }
    }

    bs_gate_map_free(map);
    printf("OK\n");
    return 0;
}

static int test_upsert_create(void)
{
    printf("  test_upsert_create ... ");

    bs_gate_chain_t* chain = bs_gate_chain_create();

    bs_gate_node_t src;
    memset(&src, 0, sizeof(src));
    src.type       = "bs_condition";
    src.field_key  = "amount";
    src.op         = "lt";
    src.value      = "50000";
    src.stable_key = "test:amount:0:threshold";
    src.layer      = BS_GATE_LAYER_DEFAULT;

    bs_gate_node_t* result = bs_gate_chain_upsert_node(chain, &src);
    if (!result) { printf("FAIL: upsert create returned NULL\n"); bs_gate_chain_free(chain); return 1; }
    if (strcmp(result->field_key, "amount") != 0) { printf("FAIL: field_key mismatch\n"); bs_gate_chain_free(chain); return 1; }

    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

static int test_upsert_overwrite(void)
{
    printf("  test_upsert_overwrite ... ");

    bs_gate_chain_t* chain = bs_gate_chain_create();

    bs_gate_node_t src;
    memset(&src, 0, sizeof(src));
    src.type = "bs_condition"; src.field_key = "amount"; src.op = "lt"; src.value = "50000";
    src.stable_key = "test:amount:0:threshold"; src.layer = BS_GATE_LAYER_DEFAULT;

    bs_gate_node_t* r1 = bs_gate_chain_upsert_node(chain, &src);
    if (!r1) { printf("FAIL: first upsert\n"); bs_gate_chain_free(chain); return 1; }

    /* Upsert again with same stable_key but different value */
    bs_gate_node_t src2;
    memset(&src2, 0, sizeof(src2));
    src2.type = "bs_condition"; src2.field_key = "amount"; src2.op = "lt"; src2.value = "100000";
    src2.stable_key = "test:amount:0:threshold"; src2.layer = BS_GATE_LAYER_DEFAULT;

    bs_gate_node_t* r2 = bs_gate_chain_upsert_node(chain, &src2);
    if (!r2) { printf("FAIL: overwrite upsert\n"); bs_gate_chain_free(chain); return 1; }
    if (r2 != r1) { printf("FAIL: overwrite should return same pointer\n"); bs_gate_chain_free(chain); return 1; }
    if (strcmp(r2->value, "100000") != 0) { printf("FAIL: overwrite value expected '100000', got '%s'\n", r2->value); bs_gate_chain_free(chain); return 1; }

    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

int main(void)
{
    printf("gate_chain_map_upsert_test:\n");
    int fail = 0;
    fail += test_map_create_lookup();
    fail += test_map_rebuild();
    fail += test_upsert_create();
    fail += test_upsert_overwrite();
    printf("  %s\n", fail ? "FAILED" : "ALL PASS");
    return fail ? 1 : 0;
}
