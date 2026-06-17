/* OPT-08: Biz_introspector tests */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bs/bs_biz_introspector.h>

static int test_introspect_null_src(void)
{
    printf("  test_introspect_null_src ... ");

    bs_biz_introspect_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.language = "C";
    char* json = NULL;
    size_t len = 0;

    int r = bs_biz_introspect(&cfg, &json, &len);
    if (r != 0) { printf("FAIL: introspect returned %d\n", r); return 1; }
    if (!json) { printf("FAIL: json is NULL\n"); return 1; }
    if (len == 0) { printf("FAIL: json length is 0\n"); free(json); return 1; }

    /* Verify valid JSON shape */
    if (!strstr(json, "\"version\"")) { printf("FAIL: missing version\n"); free(json); return 1; }
    if (!strstr(json, "\"language\"")) { printf("FAIL: missing language\n"); free(json); return 1; }
    if (!strstr(json, "\"symbols\"")) { printf("FAIL: missing symbols\n"); free(json); return 1; }

    bs_biz_introspect_free(json);
    printf("OK\n");
    return 0;
}

static int test_introspect_free_null(void)
{
    printf("  test_introspect_free_null ... ");
    bs_biz_introspect_free(NULL);
    bs_biz_introspect_free((char*)1 + 1); /* null-like, just no crash test */
    printf("OK\n");
    return 0;
}

int main(void)
{
    printf("biz_introspector_test:\n");
    int fail = 0;
    fail += test_introspect_null_src();
    fail += test_introspect_free_null();
    printf("  %s\n", fail ? "FAILED" : "ALL PASS");
    return fail ? 1 : 0;
}
