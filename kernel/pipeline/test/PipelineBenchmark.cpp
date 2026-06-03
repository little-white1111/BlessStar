#include "bs/kernel/ir/ir.h"
#include "bs/kernel/pipeline/pipeline.h"

#include <cstdio>
#include <ctime>

// Measure pipeline creation/destroy N times
static void benchmark_PipelineCreateDestroy(size_t N)
{
    clock_t start = clock();

    for (size_t i = 0; i < N; i++)
    {
        Pipeline* pipeline = bs_pipeline_create();
        bs_pipeline_destroy(pipeline);
    }

    clock_t end = clock();
    double  ms  = 1000.0 * (end - start) / CLOCKS_PER_SEC;

    printf("benchmark_PipelineCreateDestroy (N=%zu): %.3f ms\n", N, ms);
}

// Measure pipeline execution N times
static void benchmark_PipelineExecute(size_t N)
{
    Pipeline* pipeline = bs_pipeline_create();

    IRInstruction* input  = bs_ir_instruction_create("type", "name");
    Report*        output = nullptr;

    clock_t start = clock();

    for (size_t i = 0; i < N; i++)
    {
        bs_pipeline_execute(pipeline, input, &output);
    }

    clock_t end = clock();
    double  ms  = 1000.0 * (end - start) / CLOCKS_PER_SEC;

    bs_ir_instruction_destroy(input);
    bs_pipeline_destroy(pipeline);

    printf("benchmark_PipelineExecute (N=%zu): %.3f ms\n", N, ms);
}

int main()
{
    printf("=== Pipeline Performance Benchmarks ===\n");

    benchmark_PipelineCreateDestroy(10000);
    benchmark_PipelineCreateDestroy(100000);

    benchmark_PipelineExecute(10000);
    benchmark_PipelineExecute(100000);

    printf("=== Pipeline Benchmark Complete ===\n");
    return 0;
}
