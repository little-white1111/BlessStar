/* DAG version: Gate chain serialization round-trip tests */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bs/kernel/gate_chain/gate_chain_types.h>
#include <bs/kernel/gate_chain/gate_chain_serialize.h>

static int test_round_trip(void)
{
    printf("  test_round_trip ... ");

    bs_gate_chain_t* chain = bs_gate_chain_create();

    /* Build DAG: and → [c1, c2 with do] */
    bs_gate_node_t* and_node = bs_gate_node_create("bs_logic_and", "and1");

    bs_gate_node_t* c1 = bs_gate_node_create("bs_condition", "c1");
    c1->field_key = strdup("amount"); c1->op = strdup("lt"); c1->value = strdup("50000");
    c1->layer = BS_GATE_LAYER_DEFAULT;
    c1->domain = strdup("default"); c1->entity = strdup("amount");
    c1->stable_key = strdup("default:amount:0:threshold");
    c1->sub_category = strdup("threshold");
    bs_gate_node_link_child(and_node, c1);

    bs_gate_node_t* c2 = bs_gate_node_create("bs_meta_rule", "g2");
    c2->field_key = strdup("amount"); c2->op = strdup("required"); c2->value = strdup("approval_chain");
    c2->layer = BS_GATE_LAYER_POLICY;
    bs_gate_node_link_child(and_node, c2);

    /* Add DO branch to c1: condition bypass → meta action */
    bs_gate_node_t* do_action = bs_gate_node_create("bs_meta_rule", "do1");
    do_action->field_key = strdup("amount"); do_action->op = strdup("action"); do_action->value = strdup("notify");
    bs_gate_node_link_do(c1, do_action);

    chain->root = and_node;

    /* Serialize */
    char* json = NULL;
    size_t jlen = 0;
    int r = bs_gate_chain_to_json(chain, &json, &jlen);
    if (r != 0) { printf("FAIL: to_json returned %d\n", r); bs_gate_chain_free(chain); return 1; }
    if (!json || jlen == 0) { printf("FAIL: empty json\n"); bs_gate_chain_free(chain); return 1; }

    fprintf(stderr, "DEBUG JSON:\n%s\n", json);

    /* Deserialize back */
    bs_gate_chain_t* chain2 = NULL;
    r = bs_gate_chain_from_json(json, &chain2);
    if (r != 0) { fprintf(stderr, "from_json returned %d\n", r); free(json); bs_gate_chain_free(chain); return 1; }
    if (!chain2) { fprintf(stderr, "chain2 is NULL\n"); free(json); bs_gate_chain_free(chain); return 1; }

    fprintf(stderr, "chain2 node_count=%zu\n", bs_gate_chain_node_count(chain2));
    if (bs_gate_chain_node_count(chain2) == 0) {
        printf("FAIL: zero nodes after deserialize\n");
        free(json); bs_gate_chain_free(chain); bs_gate_chain_free(chain2); return 1;
    }

    /* Verify round trip: serialize again and compare lengths */
    char* json2 = NULL;
    size_t jlen2 = 0;
    r = bs_gate_chain_to_json(chain2, &json2, &jlen2);
    if (r != 0) { printf("FAIL: second to_json returned %d\n", r); free(json); bs_gate_chain_free(chain); bs_gate_chain_free(chain2); return 1; }
    if (jlen != jlen2) { printf("FAIL: json length mismatch %zu vs %zu\n", jlen, jlen2); free(json); free(json2); bs_gate_chain_free(chain); bs_gate_chain_free(chain2); return 1; }

    free(json);
    free(json2);
    bs_gate_chain_free(chain);
    bs_gate_chain_free(chain2);
    printf("OK\n");
    return 0;
}

static int test_from_legacy_format(void)
{
    printf("  test_from_legacy_format ... ");

    /* Legacy flat array format (backward compat) */
    const char* legacy_json = "{\n"
        "\"version\":\"1.0\",\n"
        "\"gates\":[\n"
        "  {\"type\":\"bs_condition\",\"id\":\"c1\",\"field_key\":\"amount\",\"op\":\"lt\",\"value\":\"50000\"},\n"
        "  {\"type\":\"bs_meta_rule\",\"id\":\"g2\",\"field_key\":\"amount\",\"op\":\"required\",\"value\":\"approval_chain\"}\n"
        "]}";

    bs_gate_chain_t* chain = NULL;
    int r = bs_gate_chain_from_json(legacy_json, &chain);
    if (r != 0) { printf("FAIL: from_json legacy returned %d\n", r); return 1; }
    if (!chain) { printf("FAIL: chain is NULL\n"); return 1; }
    if (!chain->root) { printf("FAIL: root is NULL after legacy parse\n"); bs_gate_chain_free(chain); return 1; }

    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

static int test_from_dag_format(void)
{
    printf("  test_from_dag_format ... ");

    const char* dag_json = "{\n"
        "\"version\":\"1.0\",\n"
        "\"root\":{\n"
        "  \"type\":\"bs_logic_and\",\"id\":\"and1\",\n"
        "  \"children\":[\n"
        "    {\"type\":\"bs_condition\",\"id\":\"c1\",\"field_key\":\"amount\",\"op\":\"lt\",\"value\":\"50000\"},\n"
        "    {\"type\":\"bs_condition\",\"id\":\"c2\",\"field_key\":\"dept\",\"op\":\"eq\",\"value\":\"finance\"}\n"
        "  ]\n"
        "}}";

    bs_gate_chain_t* chain = NULL;
    int r = bs_gate_chain_from_json(dag_json, &chain);
    if (r != 0) { printf("FAIL: from_json DAG returned %d\n", r); return 1; }
    if (!chain) { printf("FAIL: chain is NULL\n"); return 1; }
    if (!chain->root) { printf("FAIL: root is NULL\n"); bs_gate_chain_free(chain); return 1; }
    if (bs_gate_chain_node_count(chain) != 3) { printf("FAIL: expected 3 nodes, got %zu\n", bs_gate_chain_node_count(chain)); bs_gate_chain_free(chain); return 1; }

    bs_gate_chain_free(chain);
    printf("OK\n");
    return 0;
}

int main(void)
{
    printf("gate_chain_serialize_test:\n");
    int fail = 0;
    fail += test_round_trip();
    fail += test_from_legacy_format();
    fail += test_from_dag_format();
    printf("  %s\n", fail ? "FAILED" : "ALL PASS");
    return fail ? 1 : 0;
}
