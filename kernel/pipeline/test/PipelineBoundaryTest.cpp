#include "bs/kernel/ir/ir.h"
#include "bs/kernel/pipeline/pipeline.h"
#include "bs/kernel/report/report.h"

#include <cassert>
#include <cstdio>

static void test_Pipeline_NullInput()
{
    Report* output = nullptr;
    pipeline_execute(nullptr, nullptr, &output);
    printf("test_Pipeline_NullInput: PASS\n");
}

static void test_Pipeline_NullOutput()
{
    Pipeline*      pipeline = pipeline_create();
    IRInstruction* instr    = ir_instruction_create("type", "name");

    pipeline_execute(pipeline, instr, nullptr);

    pipeline_destroy(pipeline);
    ir_instruction_destroy(instr);
    printf("test_Pipeline_NullOutput: PASS\n");
}

static void test_Pipeline_EmptyPipeline()
{
    Pipeline*      pipeline = pipeline_create();
    IRInstruction* instr    = ir_instruction_create("type", "name");
    Report*        output   = nullptr;

    pipeline_execute(pipeline, instr, &output);

    pipeline_destroy(pipeline);
    ir_instruction_destroy(instr);
    if (output)
        report_destroy(output);
    printf("test_Pipeline_EmptyPipeline: PASS\n");
}

static void test_Pipeline_SingleInstruction()
{
    Pipeline*      pipeline = pipeline_create();
    IRInstruction* instr    = ir_instruction_create("type", "name");
    Report*        output   = nullptr;

    pipeline_execute(pipeline, instr, &output);

    pipeline_destroy(pipeline);
    ir_instruction_destroy(instr);
    if (output)
        report_destroy(output);
    printf("test_Pipeline_SingleInstruction: PASS\n");
}

static void test_Pipeline_RepeatedCreateDestroy()
{
    for (int i = 0; i < 10000; i++)
    {
        Pipeline* pipeline = pipeline_create();
        pipeline_destroy(pipeline);
    }
    printf("test_Pipeline_RepeatedCreateDestroy: PASS\n");
}

int main()
{
    printf("=== Pipeline Boundary Tests ===\n");
    test_Pipeline_NullInput();
    test_Pipeline_NullOutput();
    test_Pipeline_EmptyPipeline();
    test_Pipeline_SingleInstruction();
    test_Pipeline_RepeatedCreateDestroy();
    printf("=== All Pipeline Boundary Tests PASSED! ===\n");
    return 0;
}
