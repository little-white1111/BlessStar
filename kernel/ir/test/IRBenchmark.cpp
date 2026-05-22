#include "bs/kernel/ir/ir.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>

// Measure time (in ms) for creating N IR instructions
static void benchmark_IRInstructionCreate(size_t N)
{
    clock_t start = clock();

    for (size_t i = 0; i < N; i++)
    {
        IRInstruction* instr = ir_instruction_create("type", "name");
        ir_instruction_destroy(instr);
    }

    clock_t end = clock();
    double  ms  = 1000.0 * (end - start) / CLOCKS_PER_SEC;

    printf("benchmark_IRInstructionCreate (N=%zu): %.3f ms\n", N, ms);
}

// Measure time for adding N metadata entries
static void benchmark_IRMetadata(size_t N)
{
    clock_t start = clock();

    IRInstruction* instr = ir_instruction_create("type", "name");

    for (size_t i = 0; i < N; i++)
    {
        char key[32], value[32];
        sprintf(key, "key%zu", i);
        sprintf(value, "value%zu", i);

        IRMetadata* meta = ir_metadata_create(key, value);
        ir_instruction_add_metadata(instr, meta);
    }

    ir_instruction_destroy(instr);

    clock_t end = clock();
    double  ms  = 1000.0 * (end - start) / CLOCKS_PER_SEC;

    printf("benchmark_IRMetadata (N=%zu): %.3f ms\n", N, ms);
}

// Measure time for building a list of N instructions
static void benchmark_IRInstructionList(size_t N)
{
    clock_t start = clock();

    IRInstructionList* list = ir_instruction_list_create();

    for (size_t i = 0; i < N; i++)
    {
        IRInstruction* instr = ir_instruction_create("type", "name");
        ir_instruction_list_add(list, instr);
    }

    ir_instruction_list_destroy(list);

    clock_t end = clock();
    double  ms  = 1000.0 * (end - start) / CLOCKS_PER_SEC;

    printf("benchmark_IRInstructionList (N=%zu): %.3f ms\n", N, ms);
}

int main()
{
    printf("=== IR Performance Benchmarks ===\n");

    benchmark_IRInstructionCreate(10000);
    benchmark_IRInstructionCreate(100000);

    benchmark_IRMetadata(1000);
    benchmark_IRMetadata(10000);

    benchmark_IRInstructionList(1000);
    benchmark_IRInstructionList(10000);

    printf("=== IR Benchmark Complete ===\n");
    return 0;
}
