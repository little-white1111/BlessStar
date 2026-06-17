/* OPT-02: Gate chain serialization round-trip tests */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bs/kernel/gate_chain/gate_chain_types.h>
#include <bs/kernel/gate_chain/gate_chain_serialize.h>

static int test_round_trip(void)
{
    printf("  test_round_trip ... ");

    /* Build a gate chain manually */
    bs_gate_chain_t* chain = (bs_gate_chain_t*)calloc(1, sizeof(bs_gate_chain_t));
    chain->version = strdup("1.0");
    chain->node_count = 3;
    chain->nodes = (bs_gate_node_t*)calloc(3, sizeof(bs_gate_node_t));

    /* Node 0: condition */
    chain->nodes[0].type      = strdup("bs_condition");
    chain->nodes[0].id        = strdup("c1");
    chain->nodes[0].field_key = strdup("amount");
    chain->nodes[0].op        = strdup("lt");
    chain->nodes[0].value     = strdup("50000");
    chain->nodes[0].do_count  = 1;
    chain->nodes[0].do_ids    = (char**)calloc(1, sizeof(char*));
    chain->nodes[0].do_ids[0] = strdup("g2");

    /* Node 1: meta_rule */
    chain->nodes[1].type      = strdup("bs_meta_rule");
    chain->nodes[1].id        = strdup("g2");
    chain->nodes[1].field_key = strdup("amount");
    chain->nodes[1].op        = strdup("required");
    chain->nodes[1].value     = strdup("approval_chain");

    /* Node 2: logic_and */
    chain->nodes[2].type       = strdup("bs_logic_and");
    chain->nodes[2].id         = strdup("and1");
    chain->nodes[2].child_count = 2;
    chain->nodes[2].child_ids  = (char**)calloc(2, sizeof(char*));
    chain->nodes[2].child_ids[0] = strdup("c1");
    chain->nodes[2].child_ids[1] = strdup("g2");

    /* Serialize */
    char* json = NULL;
    size_t jlen = 0;
    int r = bs_gate_chain_to_json(chain, &json, &jlen);
    if (r != 0) { printf("FAIL: to_json returned %d\n", r); bs_gate_chain_free(chain); return 1; }
    if (!json || jlen == 0) { printf("FAIL: empty json output\n"); bs_gate_chain_free(chain); return 1; }

    fprintf(stderr, "DEBUG JSON:\n%s\n", json);

    /* Deserialize back */
    bs_gate_chain_t* chain2 = NULL;
    r = bs_gate_chain_from_json(json, &chain2);
    if (r != 0) { fprintf(stderr, "from_json returned %d\n", r); free(json); bs_gate_chain_free(chain); return 1; }
    if (!chain2) { fprintf(stderr, "chain2 is NULL\n"); free(json); bs_gate_chain_free(chain); return 1; }

    /* Verify round trip */
    fprintf(stderr, "chain2->node_count=%zu\n", chain2->node_count);
    for (size_t i = 0; i < chain2->node_count; i++) {
        fprintf(stderr, "  node[%zu]: type=%s id=%s field_key=%s op=%s value=%s\n",
                i,
                chain2->nodes[i].type ? chain2->nodes[i].type : "(null)",
                chain2->nodes[i].id ? chain2->nodes[i].id : "(null)",
                chain2->nodes[i].field_key ? chain2->nodes[i].field_key : "(null)",
                chain2->nodes[i].op ? chain2->nodes[i].op : "(null)",
                chain2->nodes[i].value ? chain2->nodes[i].value : "(null)");
    }

    if (chain2->node_count != 3) { printf("FAIL: node_count mismatch: %zu\n", chain2->node_count); bs_gate_chain_free(chain); bs_gate_chain_free(chain2); free(json); return 1; }
    if (strcmp(chain2->nodes[0].id, "c1") != 0) { printf("FAIL: node[0].id mismatch\n"); bs_gate_chain_free(chain); bs_gate_chain_free(chain2); free(json); return 1; }
    if (strcmp(chain2->nodes[0].type, "bs_condition") != 0) { printf("FAIL: node[0].type mismatch\n"); bs_gate_chain_free(chain); bs_gate_chain_free(chain2); free(json); return 1; }
    if (strcmp(chain2->nodes[0].field_key, "amount") != 0) { printf("FAIL: node[0].field_key mismatch\n"); bs_gate_chain_free(chain); bs_gate_chain_free(chain2); free(json); return 1; }
    if (strcmp(chain2->nodes[0].op, "lt") != 0) { printf("FAIL: node[0].op mismatch\n"); bs_gate_chain_free(chain); bs_gate_chain_free(chain2); free(json); return 1; }
    if (chain2->nodes[0].do_count != 1) { printf("FAIL: node[0].do_count mismatch\n"); bs_gate_chain_free(chain); bs_gate_chain_free(chain2); free(json); return 1; }
    if (strcmp(chain2->nodes[2].child_ids[0], "c1") != 0) { printf("FAIL: node[2].child_ids[0] mismatch\n"); bs_gate_chain_free(chain); bs_gate_chain_free(chain2); free(json); return 1; }

    /* Re-serialize and check for consistency */
    char* json2 = NULL;
    size_t jlen2 = 0;
    r = bs_gate_chain_to_json(chain2, &json2, &jlen2);
    if (r != 0) { printf("FAIL: second to_json returned %d\n", r); bs_gate_chain_free(chain); bs_gate_chain_free(chain2); free(json); return 1; }
    if (jlen != jlen2) { printf("FAIL: json length mismatch %zu vs %zu\n", jlen, jlen2); free(json); free(json2); bs_gate_chain_free(chain); bs_gate_chain_free(chain2); return 1; }

    free(json);
    free(json2);
    bs_gate_chain_free(chain);
    bs_gate_chain_free(chain2);
    printf("OK\n");
    return 0;
}

static int test_from_blockly_format(void)
{
    printf("  test_from_blockly_format ... ");

    /* Simulate Blockly serialization format */
    const char* blockly_json = "{\n"
        "\"version\":\"1.0\",\n"
        "\"gates\":[\n"
        "  {\"type\":\"bs_condition\",\"id\":\"c1\",\"field_key\":\"amount\",\"op\":\"lt\",\"value\":\"50000\",\"do\":[\"g2\"]},\n"
        "  {\"type\":\"bs_meta_rule\",\"id\":\"g2\",\"field_key\":\"amount\",\"op\":\"required\",\"value\":\"approval_chain\"},\n"
        "  {\"type\":\"bs_logic_and\",\"id\":\"and1\",\"children\":[\"c1\",\"g2\"]}\n"
        "]}";

    bs_gate_chain_t* chain = NULL;
    int r = bs_gate_chain_from_json(blockly_json, &chain);
    if (r != 0) { printf("FAIL: from_json returned %d\n", r); return 1; }
    if (!chain) { printf("FAIL: chain is NULL\n"); return 1; }
    if (chain->node_count != 3) { printf("FAIL: node_count expected 3, got %zu\n", chain->node_count); bs_gate_chain_free(chain); return 1; }

    /* Verify fields parsed correctly */
    if (strcmp(chain->nodes[0].type, "bs_condition") != 0) { printf("FAIL: type mismatch\n"); bs_gate_chain_free(chain); return 1; }
    if (chain->nodes[0].do_count != 1) { printf("FAIL: do_count mismatch\n"); bs_gate_chain_free(chain); return 1; }
    if (strcmp(chain->nodes[2].child_ids[0], "c1") != 0) { printf("FAIL: child_ids[0] mismatch\n"); bs_gate_chain_free(chain); return 1; }

    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

int main(void)
{
    printf("gate_chain_serialize_test:\n");
    int fail = 0;
    fail += test_round_trip();
    fail += test_from_blockly_format();
    printf("  %s\n", fail ? "FAILED" : "ALL PASS");
    return fail ? 1 : 0;
}
