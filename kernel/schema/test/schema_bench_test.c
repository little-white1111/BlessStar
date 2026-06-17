/* Schema benchmark test */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <bs/kernel/schema/schema_validator.h>
#include <bs/kernel/schema/schema_types.h>

#ifdef _WIN32
#include <windows.h>
static double get_time_ms(void)
{
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1000.0 / (double)freq.QuadPart;
}
#else
#include <sys/time.h>
static double get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
}
#endif

int main(void)
{
    printf("schema_bench_test: 500-field schema validation\n");

    /* Build 500 fields: f0..f499 */
    const int N = 500;
    bs_schema_field_def_t* fields = (bs_schema_field_def_t*)
        calloc(N, sizeof(bs_schema_field_def_t));

    for (int i = 0; i < N; i++)
    {
        char name[32];
        snprintf(name, sizeof(name), "field_%d", i);
        fields[i].name = strdup(name);
        fields[i].type     = BS_SCHEMA_TYPE_STR;
        fields[i].required = false;
        fields[i].ai_hint  = "benchmark field hint text for testing";
    }

    /* Build config: object with 500 str fields */
    bs_value_t config;
    config.type = BS_VAL_OBJ;
    config.data.obj.count = N;
    config.data.obj.fields = (bs_field_t*)calloc(N, sizeof(bs_field_t));

    for (int i = 0; i < N; i++)
    {
        char name[32];
        snprintf(name, sizeof(name), "field_%d", i);
        config.data.obj.fields[i].name = strdup(name);
        config.data.obj.fields[i].value.type = BS_VAL_STR;
        config.data.obj.fields[i].value.data.str_val = strdup("benchmark_value");
    }

    bs_schema_validate_opts_t opts;
    opts.fail_fast = false;

    bs_schema_validation_result_t res;

    /* Warmup */
    for (int w = 0; w < 10; w++)
    {
        memset(&res, 0, sizeof(res)); res.ok = 1;
        bs_schema_validate_fields(fields, N, &config, &opts, &res, "", NULL, NULL);
        bs_schema_validation_result_free(&res);
    }

    /* Timed run */
    double start = get_time_ms();
    const int ITER = 100;
    for (int iter = 0; iter < ITER; iter++)
    {
        memset(&res, 0, sizeof(res)); res.ok = 1;
        bs_schema_validate_fields(fields, N, &config, &opts, &res, "", NULL, NULL);
        bs_schema_validation_result_free(&res);
    }
    double elapsed = get_time_ms() - start;
    double avg_ms = elapsed / ITER;

    printf("  Average: %.3f ms per validation (%d iterations)\n", avg_ms, ITER);

    /* Cleanup */
    for (int i = 0; i < N; i++)
    {
        free((char*)fields[i].name);
    }
    for (int i = 0; i < N; i++)
    {
        free(config.data.obj.fields[i].name);
        bs_value_free(&config.data.obj.fields[i].value);
    }
    free(config.data.obj.fields);
    free(fields);

    if (avg_ms < 5.0)
    {
        printf("  PASS: %.3f ms < 5 ms threshold\n", avg_ms);
        return 0;
    }
    else
    {
        printf("  FAIL: %.3f ms > 5 ms threshold\n", avg_ms);
        return 1;
    }
}
